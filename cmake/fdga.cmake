set(DGA_NAME darwin_dga)

#######################
# FILTER DEPENDENCIES #
#######################

link_directories(/usr/local/lib/tensorflow/)


###################
#    EXECUTABLE   #
###################

add_executable(
    ${DGA_NAME}
    ${DARWIN_SOURCES}
    samples/fdga/DGATask.cpp samples/fdga/DGATask.hpp
    samples/fdga/Generator.cpp samples/fdga/Generator.hpp
)

target_link_libraries(
    ${DGA_NAME}
    pthread
    boost_system
    libtensorflow_cc.so
    libtensorflow_framework.so
    libexecinfo.so
    libfaupl.so
)

target_include_directories(${DGA_NAME} PUBLIC /usr/local/include/contrib/eigen/)
target_include_directories(${DGA_NAME} PUBLIC /usr/local/include/contrib/absl/)
target_include_directories(${DGA_NAME} PUBLIC samples/fdga/)
