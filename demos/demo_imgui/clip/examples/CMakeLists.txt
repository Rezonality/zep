# Clip Library
# Copyright (C) 2015-2016 David Capello

function(add_example name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} clip)
endfunction()

add_example(copy)
add_example(helloworld)
add_example(int_format)
add_example(paste)
add_example(put_image)
add_example(show_image)
