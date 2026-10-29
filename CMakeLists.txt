cmake_minimum_required(VERSION 3.10)
project(apc40_host)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(external/rtmidi)

include_directories(external/rtmidi)

add_executable(apc40_host
    src/main.cpp
    src/APC40Device.cpp
    src/APC40Controller.cpp
    src/APC40Feedback.cpp
    src/APC40BankSet.cpp
)

target_link_libraries(apc40_host rtmidi)

add_executable(list_ports src/list_ports.cpp)
target_link_libraries(list_ports rtmidi)