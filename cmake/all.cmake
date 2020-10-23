if(WIN32)
add_compile_options("$<$<CONFIG:RELWITHDEBINFO>:-DTRACY_ENABLE=1 -DMT>")
add_compile_options("$<$<CONFIG:DEBUG>:-DTRACY_ENABLE=1 -DMTd>")
add_compile_options("$<$<CONFIG:RELEASE>:-DMT>")
add_compile_options(-D_CRT_SECURE_NO_WARNINGS=1 -DGLM_ENABLE_EXPERIMENTAL -DGLM_LANG_STL11_FORCED -DGLM_FORCE_DEPTH_ZERO_TO_ONE -DNOMINMAX -D_SCL_SECURE_NO_WARNINGS=1 -D_CRT_NONSTDC_NO_WARNINGS=1 -D_SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING -D_CRT_SECURE_NO_WARNINGS=1 -D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS -D_SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING)
endif()

# Ensure Debug Flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG -DDEBUG")

# ------------------------------------------------------------------------------
# Coverage
# ------------------------------------------------------------------------------
if(ENABLE_COVERAGE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g ")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

# ------------------------------------------------------------------------------
# Google Sanitizers
# ------------------------------------------------------------------------------

if(ENABLE_ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fuse-ld=gold")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=leak")
endif()

if(ENABLE_USAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fuse-ld=gold")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
endif()

if(ENABLE_TSAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fuse-ld=gold")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
endif()

# ------------------------------------------------------------------------------
# Valgrind
# ------------------------------------------------------------------------------

set(MEMORYCHECK_COMMAND_OPTIONS "${MEMORYCHECK_COMMAND_OPTIONS} --leak-check=full")
set(MEMORYCHECK_COMMAND_OPTIONS "${MEMORYCHECK_COMMAND_OPTIONS} --track-fds=yes")
set(MEMORYCHECK_COMMAND_OPTIONS "${MEMORYCHECK_COMMAND_OPTIONS} --trace-children=yes")
set(MEMORYCHECK_COMMAND_OPTIONS "${MEMORYCHECK_COMMAND_OPTIONS} --error-exitcode=1")

# System flags
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
    message(STATUS "TARGET_PC")
    set(TARGET_PC 1)
    
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

    LIST(APPEND PLATFORM_LINKLIBS
        opengl32.lib
        winmm.lib   # SDL - sound, etc.
        version.lib # SDL - windows keyboard
        imm32.lib   # SDL - windows keyboard
    )
endif()

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
    message(STATUS "TARGET_MAC")
    
    find_package(Threads REQUIRED)
    find_package(BZip2 REQUIRED)
    find_package(ZLIB REQUIRED)

    LIST(APPEND PLATFORM_LINKLIBS
        ${BZIP2_LIBRARY}
        ${ZLIB_LIBRARY}
        dl
        "-framework CoreFoundation"
        )

    set(TARGET_MAC 1)
endif()

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)

    LIST(APPEND PLATFORM_LINKLIBS
        dl 
        Threads::Threads
        z
        )

    message(STATUS "TARGET_LINUX")
    set(TARGET_LINUX 1)
endif()

message(STATUS "System: ${CMAKE_SYSTEM}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "Debug Flags: ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "Release Flags: ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "RelWithDebInfo Flags: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
message(STATUS "Arch: ${PROCESSOR_ARCH}")

