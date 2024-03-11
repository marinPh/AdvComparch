
cmake_minimum_required(VERSION 3.0)
project(ACA C)

# Define the list of source files
set(SOURCE_FILES 
    Lib/cJSON.c
    OoO470.c
)

# Add the executable
add_executable(OhMyDIR HandMadeTests/OhMyDIR.c ${SOURCE_FILES})
add_executable(OhMyDIR2 HandMadeTests/French ${SOURCE_FILES})
