cmake_minimum_required(VERSION 3.20)
project(pg_ai_sdk CXX)

# Find PostgreSQL
find_package(PostgreSQL REQUIRED)
set(POSTGRES_VERSION 18)  # Adjust this to your PostgreSQL version

# Only if PostgreSQL is found, proceed
if(NOT PostgreSQL_FOUND)
    message(FATAL_ERROR "PostgreSQL not found. Please install PostgreSQL development files.")
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Add the ai-sdk-cpp subdirectory
# This assumes you've added it as a submodule in vendor/ai-sdk-cpp
add_subdirectory(vendor/ai-sdk-cpp)

# Create the shared library for the extension
add_library(pg_ai_sdk MODULE src/pg_ai_sdk.cpp)
set_target_properties(pg_ai_sdk PROPERTIES PREFIX "")

# Add include directories specifically for the pg_ai_sdk target
target_include_directories(pg_ai_sdk
    PUBLIC
    /usr/include/postgresql/${POSTGRES_VERSION}/server
    /usr/include/postgresql
)

# Link against PostgreSQL and ai-sdk-cpp
target_link_libraries(pg_ai_sdk
    PRIVATE
    ${PostgreSQL_LIBRARIES}
    ai::sdk
)

# Installation instructions for 'make install'
# Corrected variable names for installation paths
# Adjust the version number as needed
install(TARGETS pg_ai_sdk DESTINATION /usr/lib/postgresql/${POSTGRES_VERSION}/lib)
install(FILES pg_ai_sdk.control DESTINATION /usr/share/postgresql/${POSTGRES_VERSION}/extension)
install(FILES pg_ai_sdk--1.0.sql DESTINATION /usr/share/postgresql/${POSTGRES_VERSION}/extension)