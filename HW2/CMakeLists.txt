cmake_minimum_required(VERSION 3.10)

project(HW2)

# Add your source files here
set(SOURCES
    src/main.c
    src/VLIV470.c
    src/utils.c
)

# Add your header files here
set(HEADERS
    lib/VLIV470.h
    lib/utils.h
    lib/cJSON.h
    lib.cJSON_Utils.h
)

# Add any additional libraries or dependencies here
# set(LIBRARIES
# )

# Set the output directory for the build artifacts
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add the executable target
add_executable(main ${SOURCES} ${HEADERS})

# # Link any libraries or dependencies
# target_link_libraries(main ${LIBRARIES})