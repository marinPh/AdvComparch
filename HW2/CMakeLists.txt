
project(VLIW470 C)

set(CMAKE_CXX_STANDARD 11)

add_executable(VLIW470
        main.c
        VLIW470.c)

target_sources(VLIW470 PRIVATE
        VLIW470.c)