set(TEST_NAME darwin_test)

add_executable(
    ${TEST_NAME}
    ${DARWIN_SOURCES}
    samples/ftest/TestTask.cpp samples/ftest/TestTask.hpp
    samples/ftest/Generator.cpp samples/ftest/Generator.hpp
)

target_link_libraries(
    ${TEST_NAME}
    ${DARWIN_LIBRARIES}
)

target_include_directories(${TEST_NAME} PUBLIC samples/ftest/)