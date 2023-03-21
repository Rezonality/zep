macro(copy_existing_files_glob TARGET_PROJECT GLOBPAT DESTINATION)
  file(GLOB COPY_FILES
    ${GLOBPAT})
  foreach(FILENAME ${COPY_FILES})
    set(SRC "${FILENAME}")
    set(DST "${DESTINATION}")

    MESSAGE(STATUS "Post copy existing file: " ${SRC} ", " ${DST})
    add_custom_command(
      TARGET ${TARGET_PROJECT} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy ${SRC} ${DST}
      )
  endforeach(FILENAME)
endmacro(copy_existing_files_glob)

macro(copy_existing_files TARGET_PROJECT COPY_FILES DESTINATION)
  foreach(FILENAME ${COPY_FILES})
    set(SRC "${FILENAME}")
    set(DST "${DESTINATION}")

    MESSAGE(STATUS "Add Copy Step: " ${SRC} ", " ${DST})
    add_custom_command(
      TARGET ${TARGET_PROJECT} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy ${SRC} ${DST}
      )
  endforeach(FILENAME)
endmacro(copy_existing_files)

macro(copy_generated_file TARGET_PROJECT SRC DST)
    MESSAGE(STATUS "Post copy file: " ${SRC} ", " ${DST})
    add_custom_command(
      TARGET ${TARGET_PROJECT} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy ${SRC} ${DST}
      )
endmacro(copy_generated_file)
