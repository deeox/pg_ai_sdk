extern "C" {
    #include "postgres.h"
    #include "fmgr.h"
    #include "utils/builtins.h"
    #include "executor/spi.h"
    #include "funcapi.h"
}

#include <string>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cctype>

#include "ai/ai.h"
#include "ai/logger.h"
#include "ai/openai.h"

extern "C" {
    PG_MODULE_MAGIC;

    PG_FUNCTION_INFO_V1(generate_sql_from_text);
    PG_FUNCTION_INFO_V1(execute_and_return_json);
    PG_FUNCTION_INFO_V1(generate_sql_from_text_with_model);
    PG_FUNCTION_INFO_V1(execute_and_return_json_with_model);
}

static std::string get_api_key() {
    std::string api_key;
    std::ifstream key_file("/etc/pg_ai_sdk/api_key");
    if (key_file.is_open()) {
        std::getline(key_file, api_key);
        key_file.close();
    }

    if (api_key.empty()) {
        const char* api_key_env = std::getenv("OPENROUTER_API_KEY");
        if (api_key_env) {
            api_key = api_key_env;
        }
    }

    if (api_key.empty()) {
        elog(ERROR, "API key not found. Set OPENROUTER_API_KEY for the PostgreSQL process or create /etc/pg_ai_sdk/api_key");
    }
    return api_key;
}

static std::string validate_and_sanitize_sql(std::string generated_sql_str) {
    // Trim leading and trailing whitespace from the generated SQL
    const std::string& whitespace = " \t\n\r\f\v";
    size_t first = generated_sql_str.find_first_not_of(whitespace);
    if (std::string::npos != first)
    {
        size_t last = generated_sql_str.find_last_not_of(whitespace);
        generated_sql_str = generated_sql_str.substr(first, (last - first + 1));
    }
    else
    {
        generated_sql_str.clear(); // The string is all whitespace
    }

    std::string upper_sql = generated_sql_str;
    std::transform(upper_sql.begin(), upper_sql.end(), upper_sql.begin(),
                    [](unsigned char c){ return std::toupper(c); });

    // Safety check for potentially harmful keywords
    const std::vector<std::string> forbidden_keywords = {
        "INSERT", "UPDATE", "DELETE", "DROP", "CREATE", "ALTER", "TRUNCATE",
        "GRANT", "REVOKE", "SET ", "EXECUTE", "PERFORM",
        "PG_SLEEP", "DBLINK", "LO_IMPORT", "LO_EXPORT",
        "PG_READ_FILE", "PG_LS_DIR"
    };

    for (const auto& keyword : forbidden_keywords) {
        if (upper_sql.find(keyword) != std::string::npos) {
            elog(ERROR, "Generated query contains a forbidden keyword: %s", keyword.c_str());
        }
    }

    return generated_sql_str;
}

static std::string generate_sql_for_prompt(const char* natural_language_query, const char* model_name = "openai/gpt-oss-20b:free") {
    std::string generated_sql_str;

    elog(INFO, "pg_ai_sdk: Fetching database schema information.");
    std::string schema_info;
    if (SPI_connect() == SPI_OK_CONNECT) {
        const char* query = "SELECT table_name, column_name, data_type FROM information_schema.columns WHERE table_schema = 'public' ORDER BY table_name, ordinal_position;";
        if (SPI_execute(query, true, 0) == SPI_OK_SELECT) {
            if (SPI_processed > 0) {
                schema_info += "Schema:\n";
                for (int i = 0; i < SPI_processed; i++) {
                    char* table_name = SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 1);
                    char* column_name = SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 2);
                    char* data_type = SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 3);
                    schema_info += "Table " + std::string(table_name) + ": " + std::string(column_name) + " (" + std::string(data_type) + ")\n";
                }
            }
        }
        SPI_finish();
    } else {
        elog(ERROR, "SPI_connect failed");
    }

    std::string prompt = "Given the following database schema:\n\n" + schema_info + "\n\nGenerate a SQL query that does the following:\n" + natural_language_query;
    elog(INFO, "pg_ai_sdk: Constructed prompt for AI model.");

    std::string api_key = get_api_key();

    auto client = ai::openai::create_client(api_key, "https://openrouter.ai/api");

    elog(INFO, "pg_ai_sdk: Calling AI model to generate SQL.");
    std::string system_prompt =
        "You are an expert PostgreSQL SQL generator.\n"
        "Your ONLY task is to produce correct and optimized PostgreSQL SELECT statements based strictly on the schema and requirements provided by the user.\n"
        "\n"
        "RULES:\n"
        "1. You must output ONLY a PostgreSQL SELECT query and no other type of query. No explanations, no summaries, no analysis, no markdown.\n"
        "2. You may use complex SQL features including:\n"
        "- JOINs (inner, left, right, full)\n"
        "- CTEs (WITH clauses)\n"
        "- Window functions\n"
        "- Subqueries\n"
        "- Aggregations\n"
        "- Filtering, ordering, grouping, limits\n"
        "- JSON and array functions\n"
        "- Text search functions\n"
        "- Lateral joins\n"
        "3. You must follow standard SQL formatting:\n"
        "- UPPERCASE keywords\n"
        "- lowercase table/column names\n"
        "- Indented structure\n"
        "4. You MAY NOT include ANY inline SQL comments.\n"
        "5. The user will always provide the schema in their request.\n"
        "6. If schema is ambiguous, infer sensible table/column relationships.\n"
        "7. Never generate INSERT, UPDATE, DELETE, CREATE, or any non-SELECT SQL.\n"
        "8. Output MUST NOT end with a semicolon ';'.\n"
        "9. This output is meant for PostgreSQL, DO NOT format it with '```' that is meant for code blocks.\n"
        "\n"
        "Your output must be a single, valid PostgreSQL SELECT query that satisfies the user request exactly.\n";

    ai::GenerateOptions options(model_name, system_prompt, prompt);
    auto result = client.generate_text(options);

    if (result) {
        generated_sql_str = result.text;
        elog(INFO, "pg_ai_sdk: AI model returned generated SQL:\n%s", generated_sql_str.c_str());

        generated_sql_str = validate_and_sanitize_sql(generated_sql_str);
    } else {
        elog(ERROR, "AI query failed: %s", result.error_message().c_str());
    }
    return generated_sql_str;
}

Datum
generate_sql_from_text(PG_FUNCTION_ARGS)
{
    text* natural_language_query_text = PG_GETARG_TEXT_P(0);
    char* natural_language_query = text_to_cstring(natural_language_query_text);

    std::string generated_sql_str;

    try {
        generated_sql_str = generate_sql_for_prompt(natural_language_query);
    } catch (const std::exception& e) {
        elog(ERROR, "An exception occurred: %s", e.what());
    } catch (...) {
        elog(ERROR, "An unknown exception occurred");
    }

    text* result_text = cstring_to_text(generated_sql_str.c_str());
    PG_RETURN_TEXT_P(result_text);
}

Datum
execute_and_return_json(PG_FUNCTION_ARGS)
{
    text* natural_language_query_text = PG_GETARG_TEXT_P(0);
    char* natural_language_query = text_to_cstring(natural_language_query_text);
    text* result_text = cstring_to_text("[]"); // Default to empty JSON array

    if (SPI_connect() != SPI_OK_CONNECT)
        elog(ERROR, "SPI_connect failed");

    try {
        std::string sql = generate_sql_for_prompt(natural_language_query);

        std::string json_sql = "SELECT json_agg(row_to_json(t)) FROM (" + sql + ") AS t";
        elog(INFO, "pg_ai_sdk: Executing JSON aggregation query:\n%s", json_sql.c_str());

        if (SPI_execute(json_sql.c_str(), true, 0) == SPI_OK_SELECT && SPI_processed > 0) {
            bool isnull;
            char* json_result_str = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1);
            
            if (json_result_str)
            {
                result_text = cstring_to_text(json_result_str);
            }
        }
    } catch (const std::exception& e) {
        SPI_finish();
        elog(ERROR, "An exception occurred: %s", e.what());
    } catch (...) {
        SPI_finish();
        elog(ERROR, "An unknown exception occurred");
    }

    SPI_finish();

    PG_RETURN_TEXT_P(result_text);
}

Datum
generate_sql_from_text_with_model(PG_FUNCTION_ARGS)
{
    text* natural_language_query_text = PG_GETARG_TEXT_P(0);
    text* model_name_text = PG_GETARG_TEXT_P(1);
    char* natural_language_query = text_to_cstring(natural_language_query_text);
    char* model_name = text_to_cstring(model_name_text);

    std::string generated_sql_str;

    try {
        generated_sql_str = generate_sql_for_prompt(natural_language_query, model_name);
    } catch (const std::exception& e) {
        elog(ERROR, "An exception occurred: %s", e.what());
    } catch (...) {
        elog(ERROR, "An unknown exception occurred");
    }

    text* result_text = cstring_to_text(generated_sql_str.c_str());
    PG_RETURN_TEXT_P(result_text);
}

Datum
execute_and_return_json_with_model(PG_FUNCTION_ARGS)
{
    text* natural_language_query_text = PG_GETARG_TEXT_P(0);
    text* model_name_text = PG_GETARG_TEXT_P(1);
    char* natural_language_query = text_to_cstring(natural_language_query_text);
    char* model_name = text_to_cstring(model_name_text);
    text* result_text = cstring_to_text("[]");

    if (SPI_connect() != SPI_OK_CONNECT)
        elog(ERROR, "SPI_connect failed");

    try {
        std::string sql = generate_sql_for_prompt(natural_language_query, model_name);

        std::string json_sql = "SELECT json_agg(row_to_json(t)) FROM (" + sql + ") AS t";
        elog(INFO, "pg_ai_sdk: Executing JSON aggregation query:\n%s", json_sql.c_str());

        if (SPI_execute(json_sql.c_str(), true, 0) == SPI_OK_SELECT && SPI_processed > 0) {
            bool isnull;
            char* json_result_str = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1);
            
            if (json_result_str)
            {
                result_text = cstring_to_text(json_result_str);
            }
        }
    } catch (const std::exception& e) {
        SPI_finish();
        elog(ERROR, "An exception occurred: %s", e.what());
    } catch (...) {
        SPI_finish();
        elog(ERROR, "An unknown exception occurred");
    }

    SPI_finish();

    PG_RETURN_TEXT_P(result_text);
}