if (BUILD_DEMOS)

# Set additional project information
set(COMPANY "Zep")
set(COPYRIGHT "Copyright (c) 2019 Chris Maughan. All rights reserved.")
set(IDENTIFIER "com.zep")

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_LIST_DIR}/cmake)

include(${M3RDPARTY_DIR}/list.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/demo_common.cmake)
add_subdirectory(demo_imgui)
add_subdirectory(demo_qt)

endif()
