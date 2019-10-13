cmake_minimum_required(VERSION 3.2)

message(STATUS " CMakeLists: Packages")

project(PackageBuilder)

# Used Packages
include(ExternalProject)

# For travis, we need sudo to install the package if it is a dependent from another project build
if (WIN32)
    set (INST_CMD COMMAND ${CMAKE_COMMAND} --build . --target install)
else()
    set (INST_CMD COMMAND sudo ${CMAKE_COMMAND} --build . --target install)
endif()

ExternalProject_Add(
    MUtils
    GIT_REPOSITORY "https://github.com/cmaughan/mutils.git"
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    GIT_TAG master
    INSTALL_COMMAND ${INST_CMD}
    )

add_library(PackageBuilder STATIC builder.cpp)
add_dependencies(PackageBuilder MUtils)
