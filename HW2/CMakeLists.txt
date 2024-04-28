cmake_minimum_required(VERSION 3.10)

project(HW2)
set(CMAKE_BUILD_TYPE Debug)
# Set compiler flags for debugging
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")


# Add your source files here
set(SOURCES
    src/main.c
    src/VLIW470.c
        lib/utils.c
        lib/cJSON.c
)

# Add your header files here
set(HEADERS
    lib/VLIW470.h
    lib/utils.h
    lib/cJSON.h
    lib/cJSON_Utils.h
)

# Add any additional libraries or dependencies here
set(LIBRARIES
m
 )

# Set the output directory for the build artifacts
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add the executable target
add_executable(main ${SOURCES} ${HEADERS})

# Link any libraries or dependencies
target_link_libraries(main ${LIBRARIES})