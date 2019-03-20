cmake_minimum_required(VERSION 2.8)

project(tinydir_tests C)

INCLUDE_DIRECTORIES(..)
include_directories(cbehave)
add_subdirectory(cbehave)

################################
# Add definitions

if(MSVC)
	add_definitions(-W4 -WX -wd"4127" -wd"4102" -wd"4996")
else()
	add_definitions(-fsigned-char -Wall -W -Wshadow -Wpointer-arith -Wcast-qual -Winline -Werror -Wno-unused-label)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wstrict-prototypes")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
	if("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
	else()
		add_definitions(-Wno-pointer-to-int-cast)
	endif()
endif()

################################
# Add tests
enable_testing()

add_library(util util.c util.h)

add_executable(file_open_test file_open_test.c)
target_link_libraries(file_open_test util cbehave)
add_test(NAME file_open_test COMMAND file_open_test)

if(MSVC)
add_executable(windows_unicode_test windows_unicode_test.c)
target_link_libraries(windows_unicode_test cbehave)
add_test(NAME windows_unicode_test COMMAND windows_unicode_test)
endif()
