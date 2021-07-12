greaterThan(QT_MAJOR_VERSION, 4) {
   QT += widgets
}
QT             += multimedia
win32:LIBS     += shlwapi.lib ws2_32.lib winmm.lib User32.lib Advapi32.lib shell32.lib iphlpapi.lib version.lib
unix:!mac:LIBS += -lutil -lrt
mac:LIBS       += -framework IOKit -framework SystemConfiguration -framework Carbon
OBJECTS_DIR     = objects
MOC_DIR         = moc
MUSCLE_DIR      = ../../submodules/muscle
ZG_DIR          = ../..
INCLUDEPATH    += $$MUSCLE_DIR $$ZG_DIR/include
CONFIG         += qt warn_on rtti link_prl c++11
QMAKE_MACOSX_DEPLOYMENT_TARGET=10.10  # needed for c++11 support

TARGET = ZGChoir

DEFINES += MUSCLE_USE_CPLUSPLUS11 MUSCLE_ENABLE_ZLIB_ENCODING MUSCLE_CATCH_SIGNALS_BY_DEFAULT

exists(muscle_use_qt_threads) {
   warning("muscle_use_qt_threads detected:  MUSCLE Thread class will be a wrapper around QThread")
   DEFINES += MUSCLE_USE_QT_THREADS
}
else {
   !win32:DEFINES += MUSCLE_USE_PTHREADS
   !win32:LIBS    += -lpthread
}

!win32:LIBS  += -lz
win32:RC_FILE = ./images/bell.png.rc
mac:ICON      = ./images/bell.png.icns

# Enable C++11 support
CXXFLAGS += -std=c++11 -stdlib=libc++

win32:INCLUDEPATH += $$MUSCLE_DIR/regex/regex 

RESOURCES = ./samples/samples.qrc \
            ./images/images.qrc

MUSCLE_SOURCES = \
                 $$MUSCLE_DIR/dataio/ByteBufferDataIO.cpp             \
                 $$MUSCLE_DIR/dataio/ByteBufferPacketDataIO.cpp       \
                 $$MUSCLE_DIR/dataio/FileDataIO.cpp                   \
                 $$MUSCLE_DIR/dataio/StdinDataIO.cpp                  \
                 $$MUSCLE_DIR/dataio/SimulatedMulticastDataIO.cpp     \
                 $$MUSCLE_DIR/dataio/TCPSocketDataIO.cpp              \
                 $$MUSCLE_DIR/dataio/UDPSocketDataIO.cpp              \
                 $$MUSCLE_DIR/iogateway/AbstractMessageIOGateway.cpp  \
                 $$MUSCLE_DIR/iogateway/MessageIOGateway.cpp          \
                 $$MUSCLE_DIR/iogateway/PlainTextMessageIOGateway.cpp \
                 $$MUSCLE_DIR/iogateway/PacketTunnelIOGateway.cpp     \
                 $$MUSCLE_DIR/iogateway/ProxyIOGateway.cpp            \
                 $$MUSCLE_DIR/message/Message.cpp                     \
                 $$MUSCLE_DIR/platform/qt/QMessageTransceiverThread.cpp \
                 $$MUSCLE_DIR/reflector/AbstractReflectSession.cpp    \
                 $$MUSCLE_DIR/reflector/DataNode.cpp                  \
                 $$MUSCLE_DIR/reflector/DumbReflectSession.cpp        \
                 $$MUSCLE_DIR/reflector/SignalHandlerSession.cpp      \
                 $$MUSCLE_DIR/reflector/StorageReflectSession.cpp     \
                 $$MUSCLE_DIR/reflector/FilterSessionFactory.cpp      \
                 $$MUSCLE_DIR/reflector/ReflectServer.cpp             \
                 $$MUSCLE_DIR/reflector/ServerComponent.cpp           \
                 $$MUSCLE_DIR/regex/SegmentedStringMatcher.cpp        \
                 $$MUSCLE_DIR/regex/StringMatcher.cpp                 \
                 $$MUSCLE_DIR/regex/PathMatcher.cpp                   \
                 $$MUSCLE_DIR/regex/QueryFilter.cpp                   \
                 $$MUSCLE_DIR/syslog/SysLog.cpp                       \
                 $$MUSCLE_DIR/system/DetectNetworkConfigChangesSession.cpp \
                 $$MUSCLE_DIR/system/MessageTransceiverThread.cpp     \
                 $$MUSCLE_DIR/system/SetupSystem.cpp                  \
                 $$MUSCLE_DIR/system/SignalMultiplexer.cpp            \
                 $$MUSCLE_DIR/system/SystemInfo.cpp                   \
                 $$MUSCLE_DIR/system/Thread.cpp                       \
                 $$MUSCLE_DIR/util/ByteBuffer.cpp                     \
                 $$MUSCLE_DIR/util/Directory.cpp                      \
                 $$MUSCLE_DIR/util/FilePathInfo.cpp                   \
                 $$MUSCLE_DIR/util/MiscUtilityFunctions.cpp           \
                 $$MUSCLE_DIR/util/NetworkUtilityFunctions.cpp        \
                 $$MUSCLE_DIR/util/SocketMultiplexer.cpp              \
                 $$MUSCLE_DIR/util/String.cpp                         \
                 $$MUSCLE_DIR/util/StringTokenizer.cpp                \
                 $$MUSCLE_DIR/util/PulseNode.cpp                      \
                 $$MUSCLE_DIR/zlib/ZLibCodec.cpp                      \
                 $$MUSCLE_DIR/zlib/ZLibUtilityFunctions.cpp           \

ZG_SOURCES = $$ZG_DIR/src/ZGPeerSession.cpp                     \
             $$ZG_DIR/src/ZGDatabasePeerSession.cpp             \
             $$ZG_DIR/src/ZGStdinSession.cpp                    \
             $$ZG_DIR/src/clocksync/ZGTimeAverager.cpp          \
             $$ZG_DIR/src/discovery/common/DiscoveryUtilityFunctions.cpp

PZG_SOURCES = $$ZG_DIR/src/private/PZGHeartbeatSession.cpp      \
              $$ZG_DIR/src/private/PZGThreadedSession.cpp       \
              $$ZG_DIR/src/private/PZGHeartbeatSettings.cpp     \
              $$ZG_DIR/src/private/PZGNetworkIOSession.cpp      \
              $$ZG_DIR/src/private/PZGHeartbeatPacket.cpp       \
              $$ZG_DIR/src/private/PZGUnicastSession.cpp        \
              $$ZG_DIR/src/private/PZGDatabaseState.cpp         \
              $$ZG_DIR/src/private/PZGDatabaseStateInfo.cpp     \
              $$ZG_DIR/src/private/PZGDatabaseUpdate.cpp        \
              $$ZG_DIR/src/private/PZGConstants.cpp             \
              $$ZG_DIR/src/private/PZGBeaconData.cpp            \
              $$ZG_DIR/src/private/PZGHeartbeatPeerInfo.cpp     \
              $$ZG_DIR/src/private/PZGHeartbeatSourceState.cpp  \
              $$ZG_DIR/src/private/PZGHeartbeatThreadState.cpp

win32:MUSCLE_SOURCES += $$MUSCLE_DIR/regex/regex/regcomp.c      \
                        $$MUSCLE_DIR/regex/regex/regerror.c     \
                        $$MUSCLE_DIR/regex/regex/regexec.c      \
                        $$MUSCLE_DIR/regex/regex/regfree.c      \
                        $$MUSCLE_DIR/zlib/zlib/adler32.c        \
                        $$MUSCLE_DIR/zlib/zlib/crc32.c          \
                        $$MUSCLE_DIR/zlib/zlib/deflate.c        \
                        $$MUSCLE_DIR/zlib/zlib/gzclose.c        \
                        $$MUSCLE_DIR/zlib/zlib/gzlib.c          \
                        $$MUSCLE_DIR/zlib/zlib/gzread.c         \
                        $$MUSCLE_DIR/zlib/zlib/gzwrite.c        \
                        $$MUSCLE_DIR/zlib/zlib/inffast.c        \
                        $$MUSCLE_DIR/zlib/zlib/inflate.c        \
                        $$MUSCLE_DIR/zlib/zlib/inftrees.c       \
                        $$MUSCLE_DIR/zlib/zlib/trees.c          \
                        $$MUSCLE_DIR/zlib/zlib/zutil.c

!win32:MUSCLE_SOURCES += $$MUSCLE_DIR/dataio/FileDescriptorDataIO.cpp

mac:OBJECTIVE_SOURCES += disable_app_nap.mm
mac:LIBS              += -framework Foundation

CHOIR_SOURCES = choir.cpp              \
                ChoirWindow.cpp        \
                ChoirProtocol.cpp      \
                ChoirSession.cpp       \
                ChoirThread.cpp        \
                MusicData.cpp          \
                MusicSheet.cpp         \
                MusicSheetPlayer.cpp   \
                MusicSheetWidget.cpp   \
                NoteAssignmentsMap.cpp \
                PlaybackState.cpp      \
                Quasimodo.cpp          \
                RosterWidget.cpp

CHOIR_INCLUDES = ChoirWindow.h        \
                 MusicSheetPlayer.h   \
                 MusicSheetWidget.h   \
                 Quasimodo.h          \
                 RosterWidget.h

MUSCLE_INCLUDES = $$MUSCLE_DIR/platform/qt/QMessageTransceiverThread.h

SOURCES = $$CHOIR_SOURCES $$MUSCLE_SOURCES $$ZG_SOURCES $$PZG_SOURCES
HEADERS = $$CHOIR_INCLUDES $$MUSCLE_INCLUDES

