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
    samples/fbuffer/BufferThread.cpp samples/fbuffer/BufferThread.hpp
    samples/fbuffer/Generator.cpp samples/fbuffer/Generator.hpp
    samples/fbuffer/Connectors/AConnector.cpp samples/fbuffer/Connectors/AConnector.hpp
    samples/fbuffer/Connectors/fAnomalyConnector.cpp samples/fbuffer/Connectors/fAnomalyConnector.hpp
    samples/fbuffer/Connectors/fSofaConnector.cpp samples/fbuffer/Connectors/fSofaConnector.hpp
    samples/fbuffer/Connectors/fBufferConnector.cpp samples/fbuffer/Connectors/fBufferConnector.hpp
    samples/fbuffer/OutputConfig.cpp samples/fbuffer/OutputConfig.hpp
    toolkit/AThreadManager.cpp toolkit/AThreadManager.hpp
    toolkit/AThread.cpp toolkit/AThread.hpp
)

target_link_libraries(
    ${BUFFER_NAME}
    ${DARWIN_LIBRARIES}
)

target_include_directories(${BUFFER_NAME} PUBLIC samples/fbuffer/ samples/fbuffer/Connectors/)