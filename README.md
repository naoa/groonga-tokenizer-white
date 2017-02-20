# Goonga white tokenizer

* ``TokenWhite``

``TABLE_PAT_KEY``型の``white_words``テーブルに登録されたキーのみでトークナイズするトークナイザー。

環境変数``GRN_WHITE_TABLE_NAME``でテーブル名の変更が可能。   
もしくは``tokenizer-white.table``のコンフィグでテーブル名の変更が可能。

```
config_set tokenizer-white.table white_terms
```

```
plugin_register tokenizers/white
[[0,0.0,0.0],true]
table_create white_words TABLE_PAT_KEY ShortText
[[0,0.0,0.0],true]
load --table white_words
[
{"_key": "装置"},
{"_key": "情報"}
]
[[0,0.0,0.0],2]
tokenize TokenWhite "情報処理装置は装置である"
[
  [
    0,
    0.0,
    0.0
  ],
  [
    {
      "value": "情報",
      "position": 0,
      "force_prefix": false
    },
    {
      "value": "装置",
      "position": 1,
      "force_prefix": false
    },
    {
      "value": "装置",
      "position": 2,
      "force_prefix": false
    }
  ]
]
```

## Install

Install libgroonga-dev.

Build this tokenizer.

    % ./configure
    % make
    % sudo make install

## Usage

Register `tokenizers/white`:

    % groonga DB
    > register tokenizers/white

## License

LGPL 2.1. See COPYING for details.
