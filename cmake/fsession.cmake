set(SESSION_NAME darwin_session)

###################
#    EXECUTABLE   #
###################

add_executable(
    ${SESSION_NAME}
    ${DARWIN_SOURCES}
    samples/fsession/SessionTask.cpp samples/fsession/SessionTask.hpp
    samples/fsession/Generator.cpp samples/fsession/Generator.hpp
)

target_link_libraries(
    ${SESSION_NAME}
    ${DARWIN_LIBRARIES}
)

target_include_directories(${SESSION_NAME} PUBLIC samples/fsession/)