# PG AI SDK: Natural Language to SQL PostgreSQL Extension

This project is a PostgreSQL extension that translates natural language queries into SQL using the `ClickHouse/ai-sdk-cpp`. It allows users to query their database by asking questions in plain English.

## Project Goal

The primary goal is to create a PostgreSQL function, e.g., `generate_sql_from_text(natural_language_query TEXT)`, that:
1.  Accepts a natural language query as a text input.
2.  Introspects the current database's schema to understand available tables and columns.
3.  Constructs a prompt for an AI model, including the user's query and the schema information.
4.  Uses `ai-sdk-cpp` to send the prompt to a configured AI service (like OpenAI, Anthropic, OpenRouter, etc.).
5.  Receives the generated SQL query from the AI.
6.  Returns the SQL query as a text string.

Secondary goal is to run the generated SQL query and return the results, returning in a JSON format since generic result sets can vary widely. Also, for customisability, we can allow users to specify which AI model/provider to use via additional parameters.

## Demo Video

[![PG AI SDK Demo](Demo Video)](https://github.com/user-attachments/assets/8f10de6c-e039-44e5-a46c-9b972e7a65d4)

## Examples

### Example 1: Generate SQL only using Default Model (`openai/gpt-oss-20b:free`)
Query:
```sql
-- Generate SQL query from natural language
SELECT generate_sql_from_text('how many matches were won by Royal Challengers Bangalore is 2023');
```
Output:
```sql
INFO:  pg_ai_sdk: Received query: "how many matches were won by Royal Challengers Bangalore is 2023"
INFO:  pg_ai_sdk: Fetching database schema information.
INFO:  pg_ai_sdk: Constructed prompt for AI model.
INFO:  pg_ai_sdk: Initializing AI client.
INFO:  pg_ai_sdk: Calling AI model to generate SQL.
INFO:  pg_ai_sdk: AI model returned generated SQL:
SELECT COUNT(*) AS total_wins
  FROM matches
 WHERE season = 2023
   AND winner = 'Royal Challengers Bangalore'
```

### Example 2: Generate and Execute using Custom Model
Query:
```sql
-- Execute generated SQL and get results in JSON
SELECT pg_ai_sdk_execute_json('last 5 matches won by Royal Challengers Bangalore', 'qwen/qwen-2.5-coder-32b-instruct:free');
```
Output:
```sql
INFO:  pg_ai_sdk: Received query: "last 5 matches won by Royal Challengers Bangalore"
INFO:  pg_ai_sdk: Fetching database schema information.
INFO:  pg_ai_sdk: Constructed prompt for AI model.
INFO:  pg_ai_sdk: Initializing AI client.
INFO:  pg_ai_sdk: Calling AI model to generate SQL.
INFO:  pg_ai_sdk: AI model returned generated SQL:
SELECT match_number, match_date, city, team1, team2, winner, match_result
FROM matches
WHERE winner = 'Royal Challengers Bangalore'
ORDER BY match_date DESC
LIMIT 5
INFO:  pg_ai_sdk: Executing JSON aggregation query:
SELECT json_agg(row_to_json(t)) FROM (SELECT match_number, match_date, city, team1, team2, winner, match_result
FROM matches
WHERE winner = 'Royal Challengers Bangalore'
ORDER BY match_date DESC
LIMIT 5) AS t
```
```json
[{"match_number":65,"match_date":"2023-05-18","city":"Hyderabad","team1":"Sunrisers Hyderabad","team2":"Royal Challengers Bangalore","winner":"Royal Challengers Bangalore","match_result":null}, {"match_number":60,"match_date":"2023-05-14","city":"Jaipur","team1":"Royal Challengers Bangalore","team2":"Rajasthan Royals","winner":"Royal Challengers Bangalore","match_result":null}, {"match_number":43,"match_date":"2023-05-01","city":"Lucknow","team1":"Royal Challengers Bangalore","team2":"Lucknow Super Giants","winner":"Royal Challengers Bangalore","match_result":null}, {"match_number":32,"match_date":"2023-04-23","city":"Bengaluru","team1":"Royal Challengers Bangalore","team2":"Rajasthan Royals","winner":"Royal Challengers Bangalore","match_result":null}, {"match_number":27,"match_date":"2023-04-20","city":"Chandigarh","team1":"Royal Challengers Bangalore","team2":"Punjab Kings","winner":"Royal Challengers Bangalore","match_result":null}]
(1 row)
```

## Installation and Usage

1. **Install the Dependencies**: Ensure you have PostgreSQL installed with development packages (`postgresql-server-dev-*`), CMake, and a C++ compiler. This project requires CMake 3.20 or higher, G++ 13 and C++ 20.
2. **Add as Clickhouse AI SDK as Git Submodule**: Add the `ai-sdk-cpp` repository as a submodule to keep it self-contained.
    ```bash
    git submodule add https://github.com/ClickHouse/ai-sdk-cpp.git vendor/ai-sdk-cpp
    ```
3. **Install the Extension**: Compile and install the extension into your PostgreSQL instance.
    -  **Build the Extension**:
        ```bash
        mkdir build && cd build
        CC=gcc-13 CXX=g++-13 cmake .. -DCMAKE_BUILD_TYPE=Release
        ```
    -  **Install the Extension**:
        ```bash
        sudo make install
        ```
    -  **Configure AI SDK**: Set up API KEY in `/etc/pg_ai_sdk/api_key` file.
        ```bash
        sudo mkdir -p /etc/pg_ai_sdk
        echo "OPENROUTER_API_KEY" | sudo tee /etc/pg_ai_sdk/api_key
        sudo chmod 644 /etc/pg_ai_sdk/api_key
        ```
4.  **Create the Extension in Your Database**:
    ```sql
    CREATE EXTENSION pg_ai_sdk;
    ```
5.  **Call the Function**:
    ```sql
    -- For generating SQL only
    SELECT generate_sql_from_text('show me all users who signed up last month');
    SELECT generate_sql_from_text('what is the average order amount per user?', 'openai-gpt-4');
    -- For executing and getting results in JSON
    SELECT pg_ai_sdk_execute_json('show me the total sales per product category for the last quarter');
    SELECT pg_ai_sdk_execute_json('list all employees hired in the last year with their department names', 'anthropic-claude-sonnet-4.5');
    ```

## Plan/Architecture

1.  **PostgreSQL Extension (`.so` file)**: The core logic will be written in C++ and compiled into a shared library that PostgreSQL can load.
2.  **PostgreSQL Hooks/Functions**: We will expose a User-Defined Function (UDF) to be called from SQL.
3.  **`ai-sdk-cpp`**: This library will handle the communication with the large language model (LLM). It will be integrated as a dependency using CMake. In this project we are only using **OpenRouter** interface to connect to different LLM providers.
4.  **SPI (Server Programming Interface)**: PostgreSQL's SPI will be used to run queries against the database to fetch schema information (`information_schema.tables`, `information_schema.columns`).
