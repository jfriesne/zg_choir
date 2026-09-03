find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Multimedia)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Multimedia)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_executable(ZGChoir
   choir.cpp
   ChoirProtocol.cpp
   ChoirSession.cpp
   ChoirThread.cpp
   ChoirWindow.cpp
   MusicData.cpp
   MusicSheet.cpp
   MusicSheetPlayer.cpp
   MusicSheetWidget.cpp
   NoteAssignmentsMap.cpp
   PlaybackState.cpp
   Quasimodo.cpp
   RosterWidget.cpp
   ChoirWindow.h
   MusicSheetPlayer.h
   MusicSheetWidget.h
   Quasimodo.h
   RosterWidget.h
   samples/samples.qrc
   images/images.qrc
)

target_link_libraries(ZGChoir zg qtsupport Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Multimedia)

if (APPLE)
   target_link_libraries(ZGChoir "-framework SystemConfiguration" "-framework Carbon")
endif (APPLE)
