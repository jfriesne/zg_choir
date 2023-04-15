# Specify the minimum version for CMake
cmake_minimum_required(VERSION 3.5)

# Project's name
project(zg)

option(WITH_CPLUSPLUS17 "Enable C++17 support" ON)
if (WITH_CPLUSPLUS17)
   set (CMAKE_CXX_STANDARD 17)
   add_definitions(-DMUSCLE_USE_CPLUSPLUS17)
   if(${CMAKE_VERSION} VERSION_LESS "3.1.0")
      add_definitions(-std=c++17)   # fallback for older versions of CMake that don't know about CMAKE_CXX_STANDARD
   endif ()
   if (WIN32)
      add_definitions(-D_SILENCE_CXX17_IS_LITERAL_TYPE_DEPRECATION_WARNING)  # avoid getting spammed, I know
   endif()
else ()
   message("Note:  -DWITH_CPLUSPLUS17=OFF argument was specified:  building zg with only C++11 support")
endif (WITH_CPLUSPLUS17)

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

# No sense compiling the tests and examples folders in the muscle sub-project
option(WITH_TESTS "Enable building of mslicommon test executables" ON)
set(WITH_TESTS_SAVED "${WITH_TESTS}")
option(WITH_TESTS "Enable MUSCLE tests" OFF)
set(WITH_TESTS OFF)
set(WITH_EXAMPLES_SAVED "${WITH_EXAMPLES}")
option(WITH_EXAMPLES "Enable MUSCLE examples" OFF)
set(WITH_EXAMPLES OFF)

include_directories(include submodules/muscle)
add_subdirectory(submodules/muscle)

# But we may want to compile our own tests and examples
set(WITH_TESTS    "${WITH_TESTS_SAVED}")
set(WITH_EXAMPLES "${WITH_EXAMPLES_SAVED}")

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
