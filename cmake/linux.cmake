MESSAGE(STATUS "Linux.cmake")

find_package(OpenGL)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

include_directories(${OpenGL_INCLUDE_DIRS})
link_directories(${OpenGL_LIBRARY_DIRS})
add_definitions(${OpenGL_DEFINITIONS})

if(NOT OPENGL_FOUND)
    message(INFO " OPENGL not found - cannot build app, just tests!")
endif(NOT OPENGL_FOUND)

LIST(APPEND PLATFORM_LINKLIBS
    dl 
    Threads::Threads
    stdc++fs
    xcb
    freetype
    X11
    png16
    z
    ${OPENGL_LIBRARY}
    )
