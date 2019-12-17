set(HOSTLOOKUP_NAME darwin_hostlookup)

add_executable(
    ${HOSTLOOKUP_NAME}
    ${DARWIN_SOURCES}
    samples/fhostlookup/HostLookupTask.cpp samples/fhostlookup/HostLookupTask.hpp
    samples/fhostlookup/Generator.cpp samples/fhostlookup/Generator.hpp
)

target_link_libraries(
    ${HOSTLOOKUP_NAME}
    ${DARWIN_LIBRARIES}
)

target_include_directories(${HOSTLOOKUP_NAME} PUBLIC samples/fhostlookup/)