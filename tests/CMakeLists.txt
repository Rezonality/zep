# Tests cannot be build without the ImGui parts

SET(TEST_ROOT ${CMAKE_CURRENT_LIST_DIR})
if (BUILD_TESTS AND BUILD_IMGUI)

project(unittests)

# Evil glob
file(GLOB_RECURSE FOUND_TEST_SOURCES "${ZEP_ROOT}/src/*.test.cpp")

list(APPEND TEST_SOURCES
    ${FOUND_TEST_SOURCES}
    ${TEST_ROOT}/main.cpp
)

set(CMAKE_AUTOMOC OFF)
enable_testing()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGTEST_HAS_TR1_TUPLE=0")

set (TEST_SOURCES
    ${M3RDPARTY_DIR}/googletest/googletest/src/gtest-all.cc
    ${TEST_SOURCES}
)

add_executable (unittests ${TEST_SOURCES} )

add_dependencies(unittests Zep)

target_link_libraries (unittests PRIVATE Zep ${PLATFORM_LINKLIBS} ${CMAKE_THREAD_LIBS_INIT})

add_test(unittests unittests)

target_include_directories(unittests PRIVATE
    ${M3RDPARTY_DIR}/googletest/googletest/include
    ${M3RDPARTY_DIR}/googletest/googletest
    ${M3RDPARTY_DIR}/googletest/googlemock/include
    ${M3RDPARTY_DIR}/googletest/googlemock
    ${CMAKE_BINARY_DIR}
    ${ZEP_ROOT}/include
)

install(TARGETS unittests
    EXPORT zep-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${LIBLEGACY_INCLUDE_DIRS}
)

endif()

