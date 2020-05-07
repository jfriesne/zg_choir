# Specify the minimum version for CMake
cmake_minimum_required(VERSION 3.5)

# Project's name
project(zg)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DMUSCLE_ENABLE_ZLIB_ENCODING -DMUSCLE_CATCH_SIGNALS_BY_DEFAULT -DMUSCLE_NO_EXCEPTIONS -fno-exceptions -Wall")

if (WIN32)
   # cool
else (!WIN32)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMUSCLE_USE_PTHREADS -Wno-multichar")
   if (APPLE)
      # cool
   else (!APPLE)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
   endif (APPLE)
endif (WIN32)

include_directories(include submodules/muscle)
add_subdirectory(submodules/muscle)

file(GLOB ZG_SRCS 
   "./src/*.cpp"
   "./src/callback/*.cpp"
   "./src/connector/*.cpp"
   "./src/discovery/client/*.cpp"
   "./src/messagetree/client/*.cpp"
   "./src/messagetree/gateway/*.cpp"
   "./src/messagetree/server/*.cpp"
   "./src/private/*.cpp"
) 

add_library(zg ${ZG_SRCS})
target_link_libraries(zg muscle)

if (APPLE)
   target_link_libraries(zg "-framework IOKit")
endif (APPLE)

add_executable(test_peer ${PROJECT_SOURCE_DIR}/tests/test_peer.cpp)

target_link_libraries(test_peer zg)
