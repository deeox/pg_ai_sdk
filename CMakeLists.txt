cmake_minimum_required(VERSION 3.20)
project(pg_ai_sdk CXX)

# Find PostgreSQL
find_package(PostgreSQL 18 EXACT REQUIRED)
message(STATUS "PostgreSQL include directories: ${PostgreSQL_INCLUDE_DIRS}")
message(STATUS "PostgreSQL include directories: ${PostgreSQL_SHAREDIR}")
message(STATUS "PostgreSQL include directories: ${PostgreSQL_LIBRARIES}")

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
    /usr/include/postgresql/18/server
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
install(TARGETS pg_ai_sdk DESTINATION /usr/lib/postgresql/18/lib)
install(FILES pg_ai_sdk.control DESTINATION /usr/share/postgresql/18/extension)
install(FILES pg_ai_sdk--1.0.sql DESTINATION /usr/share/postgresql/18/extension)