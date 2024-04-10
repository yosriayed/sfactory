cmake_minimum_required(VERSION 3.28)

project(static_factory)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME} INTERFACE include)
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_20)

option(BUILD_TESTS "Build tests" OFF)

if(BUILD_TESTS)

add_subdirectory(test)

endif()
