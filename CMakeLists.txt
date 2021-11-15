# Specify the minimum version for CMake
cmake_minimum_required(VERSION 3.5)

# Project's name
project(zg)

set(CMAKE_CXX_STANDARD 11)

if (WIN32)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2 -DMUSCLE_ENABLE_ZLIB_ENCODING -DMUSCLE_CATCH_SIGNALS_BY_DEFAULT -DMUSCLE_NO_EXCEPTIONS /W3 /D_CRT_SECURE_NO_WARNINGS")
else (!WIN32)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DMUSCLE_ENABLE_ZLIB_ENCODING -DMUSCLE_CATCH_SIGNALS_BY_DEFAULT -DMUSCLE_NO_EXCEPTIONS -fno-exceptions -Wall")
endif (WIN32)

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
   "./src/clocksync/*.cpp"
   "./src/connector/*.cpp"
   "./src/discovery/*/*.cpp"
   "./src/messagetree/*/*.cpp"
   "./src/udp/*.cpp"
   "./src/private/*.cpp"
) 
if (APPLE)
   file(GLOB MM_SRCS "./src/private/*.mm")
   set(ZG_SRCS ${ZG_SRCS} ${MM_SRCS})
endif (APPLE)

add_library(zg ${ZG_SRCS})
target_link_libraries(zg muscle)
target_include_directories(zg PUBLIC include)

if (APPLE)
   target_link_libraries(zg "-framework IOKit -framework Foundation")
endif (APPLE)

option(WITH_TESTS "Enable building of zg tests" ON)
if (WITH_TESTS)
   add_executable(test_peer ${PROJECT_SOURCE_DIR}/tests/test_peer.cpp)
   target_link_libraries(test_peer zg)

   add_executable(tree_client ${PROJECT_SOURCE_DIR}/tests/tree_client.cpp)
   target_link_libraries(tree_client zg)

   add_executable(tree_server ${PROJECT_SOURCE_DIR}/tests/tree_server.cpp)
   target_link_libraries(tree_server zg)

   add_executable(discovery_client ${PROJECT_SOURCE_DIR}/tests/discovery_client.cpp)
   target_link_libraries(discovery_client zg)

   add_executable(connector_client ${PROJECT_SOURCE_DIR}/tests/connector_client.cpp)
   target_link_libraries(connector_client zg)
endif ()
