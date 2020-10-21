if (BUILD_DEMOS)

# Set additional project information
set(COMPANY "Zep")
set(COPYRIGHT "Copyright (c) 2019 Chris Maughan. All rights reserved.")
set(IDENTIFIER "com.zep")

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_LIST_DIR}/cmake)

find_package(gsl-lite REQUIRED)
find_package(SDL2 REQUIRED)
find_package(fmt REQUIRED)
find_package(gl3w REQUIRED)
find_package(imgui REQUIRED)
find_package(freetype CONFIG REQUIRED)
find_package(unofficial-concurrentqueue REQUIRED)
find_package(tinyfiledialogs REQUIRED)

include(${M3RDPARTY_DIR}/list.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/demo_common.cmake)
add_subdirectory(demo_imgui)
add_subdirectory(demo_qt)

endif()
