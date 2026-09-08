find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)

add_executable(ZGBrowser MACOSX_BUNDLE
   src/main.cpp
   src/BrowserWidget.cpp
   src/DiscoveryWidget.cpp
   src/MainWindow.cpp
   src/MessagePanel.cpp
   src/NodeTreeItem.cpp
   src/BrowserWidget.h
   src/DiscoveryWidget.h
   src/MainWindow.h
   src/MessagePanel.h
   src/MuscleQt.h
   src/NodeTreeItem.h
   src/Theme.h
   ${CMAKE_CURRENT_SOURCE_DIR}/../../submodules/muscle/platform/qt/QPostEventCallbackMechanism.h
)

target_link_libraries(ZGBrowser zg Qt${QT_VERSION_MAJOR}::Widgets)

if (APPLE)
   # macOS 15+ requires user consent before an app may send multicast/broadcast
   # traffic on the LAN, which is exactly what ZG's discovery pings are.  Without
   # this key the OS denies them silently and the systems list stays empty.
   set_target_properties(ZGBrowser PROPERTIES
      MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
      MACOSX_BUNDLE_BUNDLE_NAME "ZG Browser"
      MACOSX_BUNDLE_GUI_IDENTIFIER "org.zgchoir.zgbrowser"
      MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0")
   target_link_libraries(ZGBrowser "-framework SystemConfiguration" "-framework Carbon")
endif (APPLE)
