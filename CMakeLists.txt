# Specify the minimum version for CMake
cmake_minimum_required(VERSION 3.5)

# Project's name
project(zg)

set(CMAKE_CXX_STANDARD 11)

add_definitions(-DMUSCLE_ENABLE_ZLIB_ENCODING)
add_definitions(-DMUSCLE_USE_PTHREADS)
add_definitions(-DMUSCLE_CATCH_SIGNALS_BY_DEFAULT)

include_directories(include muscle)
add_subdirectory(muscle)

file(GLOB ZG_SRCS 
   "./src/*.cpp"
   "./src/private/*.cpp"
) 

add_library(zg ${ZG_SRCS})
target_link_libraries(zg muscle)

if (APPLE)
   target_link_libraries(zg "-framework IOKit")
endif (APPLE)

add_executable(test_peer ${PROJECT_SOURCE_DIR}/tests/test_peer.cpp)

target_link_libraries(test_peer zg)
