option(BUILD_TESTS "Determines whether to build Darwin's unit tests (using Catch2)" OFF)

set(DARWIN_TESTS_CPP
    ${PROJECT_SOURCE_DIR}/tests/units/testsBase.cpp
    ${PROJECT_SOURCE_DIR}/tests/units/testsEncoders.cpp)

if(BUILD_TESTS)
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/external/Catch2/CMakeLists.txt")
        message(FATAL_ERROR "BUILD_TESTS set to ON, but submodule is not present, please update submodule !")
    endif()

    add_executable(darwin_tests ${DARWIN_TESTS_CPP})
    add_subdirectory(${PROJECT_SOURCE_DIR}/external/Catch2)
    target_link_libraries(darwin_tests Catch2::Catch2)

    enable_testing()
    add_test(NAME tests COMMAND darwin_tests)

endif()