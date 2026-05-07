cmake_minimum_required(VERSION 3.25)

find_package(SWIG 4.0 REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
include(UseSWIG)

set_property(SOURCE imfwizard.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE imfwizard.i PROPERTY USE_TARGET_INCLUDE_DIRECTORIES ON)

swig_add_library(pyimfwizard
  TYPE SHARED
  LANGUAGE python
  SOURCES imfwizard.i
)

target_include_directories(pyimfwizard PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${Python3_INCLUDE_DIRS}
)

target_link_libraries(pyimfwizard PRIVATE
  imfwizard_core
  Python3::Python
)

set_target_properties(pyimfwizard PROPERTIES
  OUTPUT_NAME "_imfwizard"
  PREFIX ""
)

# Let CMake/SWIG handle the suffix properly
if(Python3_SOABI)
  set_target_properties(pyimfwizard PROPERTIES SUFFIX ".${Python3_SOABI}.so")
endif()

# Install the Python module
install(TARGETS pyimfwizard DESTINATION ${Python3_SITEARCH})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/imfwizard.py DESTINATION ${Python3_SITEARCH})
