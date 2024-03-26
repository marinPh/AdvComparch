
cmake_minimum_required(VERSION 3.0)
project(ACA C)

# Define the list of source files
set(SOURCE_FILES 
    Lib/cJSON.c
    OoO470.c
)
# Enable debugging information
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# Add the executable
#add_executable(OhMyDIR HandMadeTests/OhMyDIR.c ${SOURCE_FILES})
add_executable(FrenchDispatch HandMadeTests/FrenchDispatch.c ${SOURCE_FILES})
##add_executable(KingOfPop HandMadeTests/KingOfPop.c ${SOURCE_FILES})
add_executable(NameYourIssue HandMadeTests/NameYourIssue.c ${SOURCE_FILES})
add_executable(CommitmentIsScary HandMadeTests/CommitmentIsScary.c ${SOURCE_FILES})

add_executable(Simulate HandMadeTests/Simulate.c ${SOURCE_FILES})

add_custom_target(debugC
    COMMAND gdb CommitmentIsScary
    DEPENDS CommitmentIsScary
)

add_custom_target(debug
    COMMAND gdb Simulate
    DEPENDS Simulate
)
