set(INSPECTION_NAME darwin_content_inspection)

#######################
# FILTER DEPENDENCIES #
#######################

link_directories(
    ${LIBYARA_LIBRARY_DIRS}
)


###################
#    EXECUTABLE   #
###################

add_executable(
    ${INSPECTION_NAME}
    ${DARWIN_SOURCES}
    samples/finspection/ContentInspectionTask.cpp samples/finspection/ContentInspectionTask.hpp
    samples/finspection/Generator.cpp samples/finspection/Generator.hpp
    samples/finspection/data_pool.cpp samples/finspection/data_pool.hpp
    samples/finspection/file_utils.cpp samples/finspection/file_utils.hpp
    samples/finspection/hash_utils.cpp samples/finspection/hash_utils.hpp
    samples/finspection/rand_utils.cpp samples/finspection/rand_utils.hpp
    samples/finspection/flow.cpp samples/finspection/flow.hpp
    samples/finspection/packets.cpp samples/finspection/packet-utils.hpp
    samples/finspection/extract_impcap.cpp samples/finspection/extract_impcap.hpp
    samples/finspection/stream_buffer.cpp samples/finspection/stream_buffer.hpp
    samples/finspection/tcp_sessions.cpp samples/finspection/tcp_sessions.hpp
    samples/finspection/yara_utils.cpp samples/finspection/yara_utils.hpp
    samples/finspection/packet-utils.hpp
)

target_link_libraries(
    ${INSPECTION_NAME}
    ${DARWIN_LIBRARIES}
    yara
)

target_include_directories(${INSPECTION_NAME} PUBLIC ${LIBYARA_INCLUDE_DIRS})
target_include_directories(${INSPECTION_NAME} PUBLIC samples/finspection/)