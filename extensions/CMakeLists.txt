SET(EXTENSION_ROOT ${CMAKE_CURRENT_LIST_DIR})

SET(ORCA_SRC 
    ${EXTENSION_ROOT}/orca/mode_orca.cpp
    ${EXTENSION_ROOT}/orca/mode_orca.h
    ${EXTENSION_ROOT}/orca/orca.cpp
    ${EXTENSION_ROOT}/orca/orca.h
    ${EXTENSION_ROOT}/orca/syntax_orca.cpp
    ${EXTENSION_ROOT}/orca/syntax_orca.h
    )

SET(REPL_SRC 
    ${EXTENSION_ROOT}/repl/mode_repl.cpp
    ${EXTENSION_ROOT}/repl/mode_repl.h)

SET(ZEP_EXTENSION_SRC ${REPL_SRC} ${ORCA_SRC} PARENT_SCOPE)
