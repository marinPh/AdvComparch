
cmake_minimum_required(VERSION 3.0)
project(ACA C)

# Define the list of source files
set(SOURCE_FILES 
    Lib/cJSON.c
    OoO470.c
)

# Add the executable
#add_executable(OhMyDIR HandMadeTests/OhMyDIR.c ${SOURCE_FILES})
#add_executable(FrenchDispatch HandMadeTests/FrenchDispatch.c ${SOURCE_FILES})
##add_executable(KingOfPop HandMadeTests/KingOfPop.c ${SOURCE_FILES})
#add_executable(NameYourIssue HandMadeTests/NameYourIssue.c ${SOURCE_FILES})
#add_executable(CommitmentIsScary HandMadeTests/CommitmentIsScary.c ${SOURCE_FILES})

add_executable(FullyFunk HandMadeTests/FullyFunk.c ${SOURCE_FILES})
