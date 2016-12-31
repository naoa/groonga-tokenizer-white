/* Copyright(C) 2016 Naoya Murakami <naoya@createfield.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License version 2.1 as published by the Free Software Foundation.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301  USA
*/

#include <groonga/tokenizer.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __GNUC__
#  define GNUC_UNUSED __attribute__((__unused__))
#else
#  define GNUC_UNUSED
#endif

#define MAX_N_HITS 1024

const char *white_table_name = "white_words";

typedef struct {
  const char *start;
  const char *end;
  grn_tokenizer_query *query;
  grn_tokenizer_token token;
  grn_obj *white_table;
  grn_pat_scan_hit *hits;
  const char *scan_start;
  const char *scan_rest;
  unsigned int nhits;
  unsigned int current_hit;
} white_tokenizer;

static grn_obj *
white_init(grn_ctx *ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  white_tokenizer *tokenizer;
  unsigned int normalizer_flags = 0;
  grn_tokenizer_query *query;
  grn_obj *normalized_query;
  const char *normalized_string;
  unsigned int normalized_string_length;

  query = grn_tokenizer_query_open(ctx, nargs, args, normalizer_flags);
  if (!query) {
    return NULL;
  }

  tokenizer = GRN_PLUGIN_MALLOC(ctx, sizeof(white_tokenizer));
  if (!tokenizer) {
    grn_tokenizer_query_close(ctx, query);
    GRN_PLUGIN_ERROR(ctx, GRN_NO_MEMORY_AVAILABLE,
                     "[tokenizer][white] "
                     "failed to allocate memory for white_tokenizer");
    return NULL;
  }

  tokenizer->white_table = grn_ctx_get(ctx,
                                       white_table_name,
                                       strlen(white_table_name));
  if (!tokenizer->white_table) {
    GRN_PLUGIN_ERROR(ctx, GRN_NO_MEMORY_AVAILABLE,
                     "[tokenizer][white] "
                     "failed to find table (%s)", white_table_name);
    grn_tokenizer_query_close(ctx, query);
    GRN_PLUGIN_FREE(ctx, tokenizer);
    return NULL;
  }

  if (tokenizer->white_table->header.type != GRN_TABLE_PAT_KEY) {
    GRN_PLUGIN_ERROR(ctx, GRN_INVALID_ARGUMENT,
                     "[tokenizer][white] "
                     "table must be TABLE_PAT_KEY");
    grn_tokenizer_query_close(ctx, query);
    grn_obj_unlink(ctx, tokenizer->white_table);
    GRN_PLUGIN_FREE(ctx, tokenizer);
    return NULL;
  }

  tokenizer->query = query;

  normalized_query = query->normalized_query;
  grn_string_get_normalized(ctx,
                            normalized_query,
                            &normalized_string,
                            &normalized_string_length,
                            NULL);
  tokenizer->start = normalized_string;
  tokenizer->end = normalized_string + normalized_string_length;


  if (!(tokenizer->hits =
      GRN_PLUGIN_CALLOC(ctx, sizeof(grn_pat_scan_hit) * MAX_N_HITS))) {
    GRN_PLUGIN_ERROR(ctx,GRN_NO_MEMORY_AVAILABLE,
                     "[tokenizer][white] "
                     "failed to alocate memory for grn_pat_scan_hit");
    grn_tokenizer_query_close(ctx, query);
    grn_obj_unlink(ctx, tokenizer->white_table);
    GRN_PLUGIN_FREE(ctx, tokenizer);
    return NULL;
  } else {
    tokenizer->scan_rest = normalized_string;
    tokenizer->nhits = 0;
    tokenizer->current_hit = 0;
  }

  user_data->ptr = tokenizer;

  grn_tokenizer_token_init(ctx, &(tokenizer->token));
  return NULL;
}

static grn_obj *
white_next(grn_ctx *ctx, GNUC_UNUSED int nargs, GNUC_UNUSED grn_obj **args,
            grn_user_data *user_data)
{
  white_tokenizer *tokenizer = user_data->ptr;
  grn_tokenizer_status status;
  grn_id token_id = GRN_ID_NIL;
  const char *token_top = tokenizer->start;
  const char *token_tail = tokenizer->end;
  unsigned int scan_rest_length = tokenizer->end - tokenizer->scan_rest;

  if (tokenizer->current_hit >= tokenizer->nhits) {
    tokenizer->scan_start = tokenizer->scan_rest;
    if (scan_rest_length > 0) {
      tokenizer->nhits = grn_pat_scan(ctx, (grn_pat *)tokenizer->white_table,
                                      tokenizer->scan_rest,
                                      scan_rest_length,
                                      tokenizer->hits, MAX_N_HITS, &(tokenizer->scan_rest));
      tokenizer->current_hit = 0;
      scan_rest_length = tokenizer->end - tokenizer->scan_rest;
    }
  }
  if (tokenizer->nhits > 0 &&
      tokenizer->current_hit < tokenizer->nhits) {
    token_id = tokenizer->hits[tokenizer->current_hit].id;
    token_top = tokenizer->scan_start + tokenizer->hits[tokenizer->current_hit].offset;
    token_tail = token_top + tokenizer->hits[tokenizer->current_hit].length;
    tokenizer->current_hit++;
  }

  if (scan_rest_length == 0 && tokenizer->current_hit >= tokenizer->nhits) {
    status = GRN_TOKEN_LAST;
    if (tokenizer->current_hit == 0) {
      status |= GRN_TOKEN_SKIP;
    }
  } else {
    status = GRN_TOKEN_CONTINUE;
  }

  if (token_id) {
    char key_name[GRN_TABLE_MAX_KEY_SIZE];
    int key_len;
    key_len = grn_table_get_key(ctx, tokenizer->white_table, token_id,
                                key_name, GRN_TABLE_MAX_KEY_SIZE);
    grn_tokenizer_token_push(ctx,
                             &(tokenizer->token),
                             key_name,
                             key_len,
                             status);
  } else {
    status |= GRN_TOKEN_SKIP;
    grn_tokenizer_token_push(ctx,
                             &(tokenizer->token),
                             token_top,
                             token_tail - token_top,
                             status);
  }

  return NULL;
}

static grn_obj *
white_fin(grn_ctx *ctx, GNUC_UNUSED int nargs, GNUC_UNUSED grn_obj **args,
          grn_user_data *user_data)
{
  white_tokenizer *tokenizer = user_data->ptr;

  if (!tokenizer) {
    return NULL;
  }

  if (tokenizer->white_table) {
    grn_obj_unlink(ctx, tokenizer->white_table);
    GRN_PLUGIN_FREE(ctx, tokenizer->hits);
  }

  grn_tokenizer_token_fin(ctx, &(tokenizer->token));
  grn_tokenizer_query_close(ctx, tokenizer->query);
  GRN_PLUGIN_FREE(ctx, tokenizer);

  return NULL;
}


grn_rc
GRN_PLUGIN_INIT(grn_ctx *ctx)
{
  char white_table_name_env[GRN_ENV_BUFFER_SIZE];
  grn_getenv("GRN_WHITE_TABLE_NAME", white_table_name_env, GRN_ENV_BUFFER_SIZE);
  if (white_table_name_env[0]) {
    white_table_name = white_table_name_env;
  }
  return ctx->rc;
}

grn_rc
GRN_PLUGIN_REGISTER(grn_ctx *ctx)
{
  grn_rc rc;

  rc = grn_tokenizer_register(ctx, "TokenWhite", -1,
                              white_init, white_next, white_fin);

  return rc;
}

grn_rc
GRN_PLUGIN_FIN(GNUC_UNUSED grn_ctx *ctx)
{
  return GRN_SUCCESS;
}
