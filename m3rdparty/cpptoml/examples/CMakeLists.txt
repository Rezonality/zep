add_executable(parse parse.cpp)
target_link_libraries(parse cpptoml)
set_target_properties(parse PROPERTIES
  CXX_STANDARD 11
  CXX_EXTENSIONS OFF
  CXX_STANDARD_REQUIRED YES)

add_executable(cpptoml-parser parse_stdin.cpp)
target_link_libraries(cpptoml-parser cpptoml)
set_target_properties(cpptoml-parser PROPERTIES
  CXX_STANDARD 11
  CXX_EXTENSIONS OFF
  CXX_STANDARD_REQUIRED YES)

add_executable(cpptoml-build build_toml.cpp)
target_link_libraries(cpptoml-build cpptoml)
set_target_properties(cpptoml-build PROPERTIES
  CXX_STANDARD 11
  CXX_EXTENSIONS OFF
  CXX_STANDARD_REQUIRED YES)

add_executable(cpptoml-conversions conversions.cpp)
target_link_libraries(cpptoml-conversions cpptoml)
set_target_properties(cpptoml-conversions PROPERTIES
  CXX_STANDARD 11
  CXX_EXTENSIONS OFF
  CXX_STANDARD_REQUIRED YES)
