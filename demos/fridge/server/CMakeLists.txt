find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)

add_executable(FridgeServer
   main.cpp
   FridgeServerProcess.cpp
   FridgeServerWindow.cpp
   FridgeServerWindow.h
)

target_include_directories(FridgeServer PRIVATE ..)
target_link_libraries(FridgeServer zg Qt${QT_VERSION_MAJOR}::Widgets)

if (APPLE)
   target_link_libraries(FridgeServer "-framework SystemConfiguration" "-framework Carbon")
endif (APPLE)
