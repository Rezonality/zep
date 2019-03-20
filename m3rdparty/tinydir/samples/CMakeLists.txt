cmake_minimum_required(VERSION 2.6 FATAL_ERROR)
cmake_policy(VERSION 2.6)

project(tinydir)

INCLUDE_DIRECTORIES(..)

################################
# Add definitions

if(MSVC)
	add_definitions(-W4 -WX -wd"4996")
else()
	add_definitions(-fsigned-char -Wall -W -Wshadow -Wpointer-arith -Wcast-qual -Winline -Werror)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wstrict-prototypes")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
endif()

################################
# Add targets
add_executable(file_open_sample file_open_sample.c)
add_executable(iterate_sample iterate_sample.c)
add_executable(random_access_sample random_access_sample.c)
add_executable(interactive_sample interactive_sample.c)
add_executable(cpp_sample cpp_sample.cpp)
add_executable(list_to_file list_to_file.c)
