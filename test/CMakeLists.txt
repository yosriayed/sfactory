project(static_factory-tests)

include(cmake/get_cpm.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

CPMAddPackage("gh:catchorg/Catch2@3.5.3")

enable_testing()

add_executable(${PROJECT_NAME} tests.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE Catch2::Catch2WithMain static_factory)

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)

include(CTest)
include(Catch)

catch_discover_tests(${PROJECT_NAME})