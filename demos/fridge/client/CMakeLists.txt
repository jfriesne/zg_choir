find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)

add_executable(FridgeClient
   main.cpp
   ClientRosterList.cpp
   FridgeChatView.cpp
   FridgeClientCanvas.cpp
   FridgeClientWindow.cpp
   TimeSyncWidget.cpp
   ChatTextEntry.h
   ClientRosterList.h
   FridgeChatView.h
   FridgeClientCanvas.h
   FridgeClientWindow.h
   TimeSyncWidget.h
   ${CMAKE_CURRENT_SOURCE_DIR}/../../../submodules/muscle/platform/qt/QSocketCallbackMechanism.h
)

target_include_directories(FridgeClient PRIVATE ..)
target_link_libraries(FridgeClient zg Qt${QT_VERSION_MAJOR}::Widgets)

if (APPLE)
   target_link_libraries(FridgeClient "-framework SystemConfiguration" "-framework Carbon")
endif (APPLE)
