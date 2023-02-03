if (BUILD_DEMOS)

# Set additional project information
set(COMPANY "Zep")
set(COPYRIGHT "Copyright (c) 2019 Chris Maughan. All rights reserved.")
set(IDENTIFIER "com.zep")

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_LIST_DIR}/cmake)

include(${M3RDPARTY_DIR}/list.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/demo_common.cmake)

#if (WIN32)
#set(VCPKG_TARGET_TRIPLET x64-windows-static-md)
#endif()
#set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")

add_subdirectory(demo_imgui)
add_subdirectory(demo_qt)

endif()
