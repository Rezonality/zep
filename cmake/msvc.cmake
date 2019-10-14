set(COMPILER_MSVC 1)

# /WX : Linker warnings are errors 
# /GR is included by cmake and adds RTTI
# /Ehsc is included by cmake and adds exceptions 
# / wd4201 anonymous unions, used by glm among others
# /Zm127 is for precompiled header memory limit
IF (CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    STRING(REGEX REPLACE "/W[0-4]" "/W4 /WX" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
ENDIF()
        
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP /D_CRT_NONSTDC_NO_WARNINGS=1 /Zp16 /D_CRT_SECURE_NO_WARNINGS=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP /Zm127 /Zp16 /D_SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING /D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS /D_SCL_SECURE_NO_WARNINGS=1 /D_CRT_NONSTDC_NO_WARNINGS=1 /D_CRT_SECURE_NO_WARNINGS=1")
