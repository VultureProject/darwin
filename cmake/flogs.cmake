set(LOGS_NAME darwin_logs)

###################
#    EXECUTABLE   #
###################

add_executable(
    ${LOGS_NAME}
    ${DARWIN_SOURCES}
    samples/flogs/LogsTask.cpp samples/flogs/LogsTask.hpp
    samples/flogs/Generator.cpp samples/flogs/Generator.hpp
)

target_link_libraries(
    ${LOGS_NAME}
    ${DARWIN_LIBRARIES}
)

target_include_directories(${LOGS_NAME} PUBLIC samples/flogs/)