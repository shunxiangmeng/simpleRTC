cmake_minimum_required(VERSION "3.10.1")

project(simplertc LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)

if(UNIX)
elseif(WIN32)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/third_party)

file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)

add_executable(simplertc ${SOURCES})
