DEFINES += -DMUSCLE_ENABLE_ZLIB_ENCODING 
DEFINES += -DMUSCLE_USE_PTHREADS 
DEFINES += -DMUSCLE_CATCH_SIGNALS_BY_DEFAULT 

MUSCLEDIR = ../submodules/muscle

INCLUDEDIRS = -I../include -I$(MUSCLEDIR) -I$(MUSCLEDIR/zlib/zlib)
CFLAGS = $(INCLUDEDIRS) -g -Wall
GCCFLAGS = -fno-exceptions -DMUSCLE_NO_EXCEPTIONS
CXXFLAGS = $(CFLAGS) -O3 $(DEFINES)

# Uncomment these to enable clang++'s static analyzer (for more thorough error checking)
#CXXFLAGS += --analyze -Xanalyzer -analyzer-output=text
#CFLAGS   += --analyze -Xanalyzer -analyzer-output=text

LFLAGS      =  
LIBS        = -lpthread
EXECUTABLES = test_peer test_udp_multicast_transceiver tree_server tree_client connector_client discovery_client
ZLIBOBJS    = adler32.o deflate.o trees.o zutil.o inflate.o inftrees.o inffast.o crc32.o compress.o gzclose.o gzread.o gzwrite.o gzlib.o
MUSCLEOBJS  = Message.o AbstractMessageIOGateway.o MessageIOGateway.o String.o SocketMultiplexer.o NetworkUtilityFunctions.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o ZLibCodec.o SetupSystem.o ByteBufferDataIO.o FileDataIO.o StdinDataIO.o TCPSocketDataIO.o UDPSocketDataIO.o SimulatedMulticastDataIO.o FileDescriptorDataIO.o MiscUtilityFunctions.o QueryFilter.o FilePathInfo.o ReflectServer.o StringMatcher.o ServerComponent.o AbstractReflectSession.o Thread.o Directory.o SignalHandlerSession.o SignalMultiplexer.o PlainTextMessageIOGateway.o DumbReflectSession.o StorageReflectSession.o PathMatcher.o DataNode.o ZLibUtilityFunctions.o DetectNetworkConfigChangesSession.o PacketTunnelIOGateway.o SegmentedStringMatcher.o
REGEXOBJS   = 
ZGOBJS      = ZGPeerSession.o ZGStdinSession.o ZGDatabasePeerSession.o ZGTimeAverager.o DiscoveryUtilityFunctions.o
PZGOBJS     = PZGHeartbeatSession.o PZGThreadedSession.o PZGHeartbeatSettings.o PZGNetworkIOSession.o PZGHeartbeatPacket.o PZGUnicastSession.o PZGDatabaseState.o PZGDatabaseStateInfo.o PZGDatabaseUpdate.o PZGConstants.o PZGBeaconData.o PZGHeartbeatPeerInfo.o PZGHeartbeatThreadState.o PZGHeartbeatSourceState.o
ZGTREECOMMONOBJS = ITreeGatewaySubscriber.o DummyTreeGateway.o ProxyTreeGateway.o MuxTreeGateway.o NetworkTreeGateway.o
ZGTREESERVEROBJS = MessageTreeDatabasePeerSession.o MessageTreeDatabaseObject.o UndoStackMessageTreeDatabaseObject.o ServerSideMessageTreeSession.o ServerSideMessageUtilityFunctions.o DiscoveryServerSession.o ClientDataMessageTreeDatabaseObject.o
ZGTREECLIENTOBJS = ClientSideMessageTreeSession.o SystemDiscoveryClient.o ClientConnector.o MessageTreeClientConnector.o TestTreeGatewaySubscriber.o
ZGCALLBACKOBJS = SocketCallbackMechanism.o ICallbackMechanism.o
ZGUDPOBJS  = UDPMulticastTransceiver.o
SRCDIR = ../src
VPATH = . $(SRCDIR) $(SRCDIR)/private $(SRCDIR)/messagetree/gateway $(SRCDIR)/messagetree/client $(SRCDIR)/messagetree/server $(SRCDIR)/clocksync $(SRCDIR)/connector $(SRCDIR)/discovery/server $(SRCDIR)/discovery/client $(SRCDIR)/discovery/common $(SRCDIR)/callback $(SRCDIR)/udp $(MUSCLEDIR)/hashtable $(MUSCLEDIR)/message $(MUSCLEDIR)/iogateway $(MUSCLEDIR)/reflector $(MUSCLEDIR)/regex $(MUSCLEDIR)/util $(MUSCLEDIR)/syslog $(MUSCLEDIR)/system $(MUSCLEDIR)/dataio $(MUSCLEDIR)/zlib $(MUSCLEDIR)/zlib/zlib $(MUSCLEDIR)/zlib/zlib/contrib/minizip

# if the OS type variable is unset, try to set it using the uname shell command 
ifeq ($(OSTYPE),) 
  OSTYPE = $(shell echo $(strip $(shell uname)) | tr A-Z a-z)
  #$(info OSTYPE="$(OSTYPE)")
endif 

ifneq (,$(findstring g++,$(CXX)))
    CXXFLAGS += $(GCCFLAGS) $(CCOPTFLAGS)
else
    CXXFLAGS += $(CCOPTFLAGS)
endif

ifeq ($(OSTYPE),freebsd4.0) 
   CXXFLAGS += -I/usr/include/machine 
endif 

ifeq ($(OSTYPE),darwin) 
   LIBS += -framework IOKit -framework Carbon -framework SystemConfiguration

   # Enable C++11 support
   CXXFLAGS += -std=c++11 -stdlib=libc++ -DMUSCLE_USE_CPLUSPLUS11
endif 

ifeq ($(OSTYPE),linux)
   LIBS += -lutil

   # Enable C++11 support
   CXXFLAGS += -std=c++11 -DMUSCLE_USE_CPLUSPLUS11
endif 

# Makes all the programs that can be made using just cross-platform code
all : $(EXECUTABLES)

test_peer : $(ZLIBOBJS) $(MUSCLEOBJS) $(REGEXOBJS) $(ZGOBJS) $(PZGOBJS) test_peer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

test_udp_multicast_transceiver : $(ZLIBOBJS) $(MUSCLEOBJS) $(REGEXOBJS) $(ZGOBJS) $(PZGOBJS) $(ZGUDPOBJS) $(ZGCALLBACKOBJS) test_udp_multicast_transceiver.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

tree_server : $(ZLIBOBJS) $(MUSCLEOBJS) $(REGEXOBJS) $(ZGOBJS) $(PZGOBJS) $(ZGTREECOMMONOBJS) $(ZGTREESERVEROBJS) tree_server.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

tree_client : $(ZLIBOBJS) $(MUSCLEOBJS) $(REGEXOBJS) $(ZGOBJS) $(PZGOBJS) $(ZGTREECOMMONOBJS) $(ZGTREECLIENTOBJS) $(ZGCALLBACKOBJS) tree_client.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

connector_client : $(ZLIBOBJS) $(MUSCLEOBJS) $(REGEXOBJS) $(ZGOBJS) $(PZGOBJS) $(ZGTREECOMMONOBJS) $(ZGTREECLIENTOBJS) $(ZGCALLBACKOBJS) connector_client.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

discovery_client : $(ZLIBOBJS) $(MUSCLEOBJS) $(REGEXOBJS) $(ZGOBJS) $(PZGOBJS) $(ZGTREECOMMONOBJS) $(ZGTREECLIENTOBJS) $(ZGCALLBACKOBJS) discovery_client.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

clean :
	rm -f *.o *.xSYM $(EXECUTABLES)
