include_directories(.)

if(MSVC)
	add_definitions(-wd"4127" -wd"4102" -wd"4996")
else()
	if(NOT BEOS AND NOT HAIKU)
		add_definitions(-Wno-unused-label)
	endif()
endif()

add_library(cbehave STATIC cbehave.h cbehave.c apr_ring.h rlutil/rlutil.h)
