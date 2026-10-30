cmake_minimum_required(VERSION 3.10)
project(apc40_host)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(APC40_BUILD_HOST "Build the RtMidi hardware host" ON)

set(APC40_SOURCES
    src/APCControl.cpp
    src/APC40Device.cpp
    src/APC40Controller.cpp
    src/APC40Feedback.cpp
    src/APC40BankSet.cpp
)

add_library(apc40_core STATIC ${APC40_SOURCES})
target_include_directories(apc40_core PUBLIC src external/foundling/include)

if(APC40_BUILD_HOST)
    add_subdirectory(external/rtmidi)
    add_executable(apc40_host src/main.cpp src/example_app.cpp src/APC40RtMidiTransport.cpp
        src/example_app.h)
    target_link_libraries(apc40_host apc40_core rtmidi)

    add_executable(list_ports src/list_ports.cpp)
    target_link_libraries(list_ports rtmidi)

    get_target_property(rtmidi_type rtmidi TYPE)
    if(WIN32 AND rtmidi_type STREQUAL "SHARED_LIBRARY")
        foreach(runtime_target apc40_host list_ports)
            add_custom_command(TARGET ${runtime_target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:rtmidi> $<TARGET_FILE_DIR:${runtime_target}>)
        endforeach()
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(apc40_tests tests/apc40_tests.cpp)
    target_link_libraries(apc40_tests apc40_core)
    add_test(NAME apc40_native COMMAND apc40_tests)
endif()