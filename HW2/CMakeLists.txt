
project(VLIW470 C)

set(CMAKE_CXX_STANDARD 11)

add_executable(VLIW470
        main.c
        VLIW470.c
        lib/cJSON.c
        lib/utils.c)

