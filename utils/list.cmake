LIST(APPEND COMMON_INCLUDE
    utils
    utils/graphics3d
    utils/graphics3d/components
    utils/graphics2d
)

LIST(APPEND COMMON_SOURCES

utils/ui/ui_manager.cpp
utils/ui/ui_manager.h

utils/file/runtree.cpp
utils/file/runtree.h

utils/utils.h

utils/entities/entities.h 
utils/entities/entities.cpp

utils/callback.cpp

utils/file/file.cpp
utils/animation/timer.cpp
utils/string/stringutils.cpp
utils/string/tomlutils.cpp
utils/math/mathutils.cpp
utils/graphics3d/ui/camera_manipulator.h
utils/graphics3d/ui/camera_manipulator.cpp
utils
utils/utils.h
utils/list.cmake
)

