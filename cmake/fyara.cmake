set(YARA_NAME darwin_yara)

#######################
# FILTER DEPENDENCIES #
#######################

find_package(Yara REQUIRED)

# Search for static OpenSSL libs, then fall back to dynamic ones
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL QUIET)
if(NOT OpenSSL_FOUND)
    set(OPENSSL_USE_STATIC_LIBS FALSE)
    find_package(OpenSSL)
endif()

###################
#    EXECUTABLE   #
###################

add_executable(
    ${YARA_NAME}
    ${DARWIN_SOURCES}
    samples/fyara/Generator.cpp samples/fyara/Generator.hpp
    samples/fyara/YaraTask.cpp samples/fyara/YaraTask.hpp
    toolkit/Yara.cpp toolkit/Yara.hpp
)

target_link_libraries(
    ${YARA_NAME}
    ${DARWIN_LIBRARIES}
    Yara::Yara
    OpenSSL::Crypto
)

target_include_directories(${YARA_NAME} PUBLIC samples/fyara/)
