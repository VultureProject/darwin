set(DARWIN_TESTS_CPP
    ${PROJECT_SOURCE_DIR}/tests/units/testsBase.cpp
    ${PROJECT_SOURCE_DIR}/tests/units/testsToolkitEncoders.cpp)

if(BUILD_TESTS)
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/external/Catch2/CMakeLists.txt")
        message(FATAL_ERROR "BUILD_TESTS set to ON, but submodule is not present, please update submodule !")
    endif()

    add_executable(tests_toolkit ${DARWIN_TESTS_CPP})
    add_subdirectory(${PROJECT_SOURCE_DIR}/external/Catch2)
    target_link_libraries(tests_toolkit Catch2::Catch2)

    enable_testing()
    add_test(NAME tests COMMAND tests_toolkit)

endif()