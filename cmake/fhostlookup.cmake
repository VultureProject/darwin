add_executable(
    darwin_hostlookup
    ${DARWIN_SOURCES}
    samples/fhostlookup/HostLookupTask.cpp samples/fhostlookup/HostLookupTask.hpp
    samples/fhostlookup/Generator.cpp samples/fhostlookup/Generator.hpp
)

target_link_libraries(
    darwin_hostlookup
    pthread
    boost_system
)

target_include_directories(darwin_hostlookup PUBLIC samples/fhostlookup/)