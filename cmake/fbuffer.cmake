set(BUFFER_NAME darwin_buffer)

#######################
# FILTER DEPENDENCIES #
#######################

###################
#    EXECUTABLE   #
###################

add_executable(
    ${BUFFER_NAME}
    ${DARWIN_SOURCES}
    samples/fbuffer/BufferTask.cpp samples/fbuffer/BufferTask.hpp
    samples/fbuffer/BufferThreadManager.cpp samples/fbuffer/BufferThreadManager.hpp
    samples/fbuffer/Generator.cpp samples/fbuffer/Generator.hpp
    samples/fbuffer/Connectors/AConnector.cpp samples/fbuffer/Connectors/AConnector.hpp
    samples/fbuffer/Connectors/fAnomalyConnector.cpp samples/fbuffer/Connectors/fAnomalyConnector.hpp
    samples/fbuffer/Connectors/fSofaConnector.cpp samples/fbuffer/Connectors/fSofaConnector.hpp
)

target_link_libraries(
    ${BUFFER_NAME}
    ${DARWIN_LIBRARIES}
)

target_include_directories(${BUFFER_NAME} PUBLIC samples/fbuffer/ samples/fbuffer/Connectors/)