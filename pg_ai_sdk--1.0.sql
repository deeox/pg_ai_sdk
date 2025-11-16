-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_ai_sdk" to load this file. \quit

-- Register the function that will be called from SQL
CREATE OR REPLACE FUNCTION generate_sql_from_text(natural_language_query TEXT)
RETURNS TEXT
AS 'MODULE_PATHNAME', 'generate_sql_from_text'
LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION pg_ai_sdk_execute_json(
    natural_language_query text
)
RETURNS text
AS 'MODULE_PATHNAME', 'execute_and_return_json'
LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION generate_sql_from_text(natural_language_query TEXT, model_name TEXT)
RETURNS TEXT
AS 'MODULE_PATHNAME', 'generate_sql_from_text_with_model'
LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION pg_ai_sdk_execute_json(
    natural_language_query text,
    model_name TEXT
)
RETURNS text
AS 'MODULE_PATHNAME', 'execute_and_return_json_with_model'
LANGUAGE C STRICT;