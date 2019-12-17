set(CONNECTION_NAME darwin_connection)

###################
#    EXECUTABLE   #
###################

add_executable(
    ${CONNECTION_NAME}
    ${DARWIN_SOURCES}
    samples/fconnection/ConnectionSupervisionTask.cpp samples/fconnection/ConnectionSupervisionTask.hpp
    samples/fconnection/Generator.cpp samples/fconnection/Generator.hpp
)

target_link_libraries(
    ${CONNECTION_NAME}
    ${DARWIN_LIBRARIES}
)

target_include_directories(${CONNECTION_NAME} PUBLIC samples/fconnection/)