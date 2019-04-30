INCLUDE_DIRECTORIES(
    demo_qt
    ${Qt5Widgets_INCLUDE_DIRS}
    ${Qt5Gui_INCLUDE_DIRS}
    ${Qt5Core_INCLUDE_DIRS}
    )


LIST(APPEND RESOURCE_FOLDER 
    demo_qt/res)

LIST(APPEND RESOURCE_FILES
    demo_qt/app.qrc
    demo_qt/app.rc)

SET (DEMO_SOURCE_QT
    demo_qt/mainwindow.cpp
    demo_qt/mainwindow.h
    demo_qt/resource.h
    demo_qt/app.exe.manifest
    demo_qt/main-qt.cpp
    demo_qt/list.cmake
    demo_qt/app.rc
    demo_qt/common-qt.cpp
    demo_qt/common-qt.h
)

