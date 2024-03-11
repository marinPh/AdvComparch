cmake_minimum_required(VERSION 3.10)
project(ParserTest)

# Add cJSON library files to your project
set(cJSON_SOURCE_FILES cJSON.c)
set(cJSON_HEADER_FILES cJSON.h)

# Add executable target
add_executable(ParserTest ParserTest.c)

# Include cJSON header files
target_include_directories(ParserTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link cJSON source files to your executable
target_sources(ParserTest PRIVATE ${cJSON_SOURCE_FILES} ${cJSON_HEADER_FILES})
