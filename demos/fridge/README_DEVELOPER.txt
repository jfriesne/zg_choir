Note:  This document gives a programmer's view of how the FridgeClient
and FridgeServer applications work internally.  If you are just looking
for an explanation of what these apps do (from a user's perspective),
please see the README_USER.txt file in this folder instead.


I. FridgeClient
---------------

FridgeClient client is the client application that displays the
refrigerator-magnets GUI to the user.

FridgeClient has three basic tasks to perform:

  a) Display a list of available fridge-systems so that the
     user can choose which fridge-system to connect to

  b) Connect to a server within the designated fridge-system
     and subscribe to the refrigerator-magnets portion of that
     database so that the GUI can be updated appropriately as
     the database changes

  c) React to mouse events to the user by sending update-requests
     to the database.

A. Interacting with the GUI event loop via Callback Mechanisms

Below I'll talk about how each of those tasks is implemented,
but first, a small digression into software design.  Data
usually needs to flow in two directions in a client/server --
the client needs to be able to send requests to the server,
and the server need to be able to send updates back to the client.

Sending data from the client to the server is straightforward
-- you simply find the appropriate C++ method to call on the appropriate
ZGChoir C++ object, and you call it.  ZGChoir's APIs are designed to be 
non-blocking in nature, so your GUI thread can call ZGChoir methods 
freely without having to worry about them not returning in a timely 
manner.

Getting updates back from the server is a little bit trickier, because
these updates come in asynchronously with respect to what your GUI API's
event-loop is doing, and therefore we need some mechanism by which
the updates can be received by the GUI thread in a thread-safe manner,
and without every blocking the GUI thread from handling its GUI-management
tasks.

To implement this functionality, ZGChoir supplies the ICallbackMechanism
interface class (found in zg_choir/include/callback/ICallbackMechanism.h).
ICallbackMechanism is an abstract base class representing a mechanism that
allows a network thread (or any thread, really) to safely call its 
SignalDispatchMethod() when it wants the main/GUI thread to wake up
and check its ZGChoir-incoming-events queue.  The subclass of ICallbackMechanism
implements SignalDispatchThread() in some appropriate manner, so that 
shortly after SignalDispatchThread() is called, the main thread will
respond by calling DispatchCallbacks().  DispatchCallbacks() will, in turn,
call all of the various callback methods that need calling -- and since
DisptachCallbacks() is being called from within the main/GUI thread, there
are no thread-safety issues to worry about.

Different operating systems will use different mechanisms to implement
the above, but many (most?) operating systems support TCP sockets, so
as a convenience, ZGChoir supplies a SocketCallbackMechanism subclass
that implements the signalling functionality using a socket-pair.
Therefore, for many applications, a single SocketCallbackMechanism object
should be all you need in order to get callback in your GUI thread.
You can simply instantiate the SocketCallbackMechanism object near
the top of main(), and use your GUI API's socket-monitoring mechanism
to monitor socket returned by SocketCallbackMechanism::GetDispatchThreadNotifierSocket()
is ready-for-read.  When you get that notification, you need to respond to
it by calling DispatchSockets() on the SocketCallbackMechanism object.

For Qt-based GUI applications, I go a little further and supply a
QtCallbackMechanism class (in zg_choir/include/platform/qt/QtCallbackMechanism.h)
which subclasses SocketCallbackMechanism and handles all of the socket-monitoring
internally.  With this class, all a Qt-based GUI needs to do is instantiate
the object and the rest is handled automatically.  For other (non-Qt-based)
GUI APIs, it's usually possible to create a similar subclass that would
do the same thing for them.


B. System discovery

When a client like FridgeClient starts up, usually the first thing it will
want to do is present the user with a list of systems that are available on
the LAN for the client to connect to.  Note that we deliberately want the
user to choose from a list of systems, not a list of individual servers --
that way individual servers can be added to (or removed from) the system
at any time without interfering with the client's functionality.

Furthermore, the available-systems-list in the client's GUI should be self-updating;
i.e. if a new system becomes available while the list is being shown, the new system
should automatically appear in the list; and if an existing system goes away while
the list is visible, that option should automatically disappear from the list.  In
particular, the user should not be required to click on a "Refresh" button to see
the current state of who is on the network.

To implement this functionality, ZGChoir supplies the SystemDiscoveryClient
class (found in zg_choir/include/discovery/client/SystemDiscoveryClient.h).

To use it, you would instantiate a SystemDiscoveryClient object and pass
it a pointer to your GUI thread's ICallbackMechanism object (so that the
SystemDiscoveryClient object can perform callbacks) along with a signature-pattern
string.  A signature-pattern string is just a string identifying what type(s)
of ZGChoir-based server you are interested in.  For example, FridgeClient passes
FRIDGE_PROGRAM_SIGNATURE (a.k.a "Fridge") since it wants only to list systems
made out of FridgeServers and not e.g. ZGChoir-app servers or any other kind of
server.

With your SystemDiscoveryClient object available, you can subclass your
available-systems list-view class (or any class you want) from ZGChoir's
IDiscoveryNotificationTarget interface (found in 
zg/include/discovery/client/IDiscoveryNotificationTarget.h), and
implement the DiscoveryUpdate(const String &, const MessageRef &) method 
it requires.  

Make sure you pass a pointer to the SystemDiscoveryClient
object to the IDiscoveryNotificationTarget superclass-constructor as part of
your own constructor, or if you can't do that due to order-of-creation issues, 
you can pass NULL to the constructor and instead manually call 
SetDiscoveryClient(&myDiscoveryClient) on your subclass-object later on.

The final thing to do is to call Start() on the SystemDiscoveryClient
object to start its internal discovery-thread going.

Then your subclass's DiscoveryUpdate() method will be
called whenever the SystemDiscoveryClient needs to inform you about the
presence (or absence, or change in the composition of) a matching system.

As a final note, it's highly recommended that when the user selects
a system to connect to, you call Stop() on your SystemDiscoveryObject
(or just delete it) so that multicast-pings don't continue to be
sent out across the LAN every 500mS.  They won't hurt anything, exactly,
but they do put additional load on the network and on the CPU of every
server that receives them, so it's better to only be sending them when
you actually need to know the resulting information right now.


C. Connecting to a system

Once the user has selected a system to connect to, the next task
for the client is to actually connect to the system.

For that task ZGChoir supplies the ClientConnector class (in
zg_choir/include/connector/ClientConnector.h).  However,
the ClientConnector class is a bare-bones/general-purpose TCP-connector class
and not that useful on its own, since it doesn't provide any communication
semantics other than TCP connections and disconnections.  So instead
of describing the ClientConnector class in detail, I'm going to skip
down to its more-useful subclass, MessageTreeClientConnector (found
in zg_choir/include/messagetree/client/MessageTreeClientConnector.h).

MessageTreeClientConnector functions both as a TCP-connection mechanism,
and as a MuxTreeGateway that can be used to share the functionality of
that TCP-connection across multiple (i.e. dozens or even hundreds) of
software objects, such that they can all access the server-side database
simultaneously through the single TCP connection, without getting in each 
other's way.

The MessageTreeClientConnector's constructor looks like this:

   MessageTreeClientConnector(
      ICallbackMechanism * mechanism, 
      const String & signaturePattern, 
      const String & systemNamePattern, 
      const ConstQueryFilterRef & optAdditionalCriteria = ConstQueryFilterRef()
   );

The first argument is, as before, a pointer to your GUI thread's ICallbackMechanism
object, so that the MessageTreeClientConnector will be able to schedule callback
events inside your GUI thread.

The second argument is the signature-pattern of the type(s) of system you want to
connect to; in general this will a hard-coded string specifying your program's type
(e.g. FridgeClient specifies FRIDGE_PROGRAM_SIGNATURE a.k.a "Fridge" because it
only wants to connect to FridgeServers).

The third agument is the system name you want to connect to; the string you
pass in here will be based on what the user chose in response to the online-systems
dialog in the previous section.

The final argument is optional; you could use it if you wanted to further specify
which servers within the specified system were acceptable for connecting to.  For
now there is no reason to use it, since all the servers in a given system should
be equally acceptable to connect to.

Once you have your MessageTreeCallbackConnector object created, all that remains
to do is call Start() on it, and that will start the network I/O thread running.
The network I/O thread will try to connect to a server in the specified system,
and once connected, if the connection is ever broken for any reason, the I/O thread
will try to find another server to connect to instead.  That will continue until
you call Stop() on the MessageTreeCallbackConnector object (which btw you are
required to do before deleting it, otherwise you will get an assertion failure)


D. Client-side monitoring of the server's database

Once your MessageTreeCallbackConnector is running, you'll want to use it
to actually get some work done.  The way this is handled is that MessageTreeClientConnector
is a subclass of the MuxTreeGateway class, and the MuxTreeGateway class implements
the ITreeGateway class (found in zg/include/messagetree/gateway/ITreeGateway.h),
which means that the MessageTreeCallbackConnector can server as a "gateway to
the server" for any number of client-side software objects to use simultaneously.

To allow your favorite class to use the gateway, all you need to do is make your
class a subclass of the ITreeGatewaySubscriber class (found in
zg/include/messagetree/gateway/ITreeGatewaySubscriber.h), and pass a pointer
to the MessageTreeCallbackConnector object up from your constructor to the
ITreeGatewaySubscriber superclass constructor.  (Or if you can't do that, due to order-
of-construction issues, you can pass NULL to the ITreeGatewaySubscriber superclass
constructor and then explicitly call SetGateway(pointerToTheGateway) on your object
later on.

Once you've done that, all of the MessageTree database-functionality is available
to your subclass-object via the methods declared in ITreeGatewaySubscriber.h
That functionality includes methods that are there for you to call, such as:

   virtual status_t AddTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef = ConstQueryFilterRef(), TreeGatewayFlags flags = TreeGatewayFlags());
   virtual status_t RemoveTreeSubscription(const String & subscriptionPath, const ConstQueryFilterRef & optFilterRef = ConstQueryFilterRef(), TreeGatewayFlags flags = TreeGatewayFlags());
   virtual status_t RemoveAllTreeSubscriptions(TreeGatewayFlags flags = TreeGatewayFlags());
   virtual status_t UploadTreeNodeValue(const String & nodePath, const MessageRef & optPayload, TreeGatewayFlags flags = TreeGatewayFlags(), const char * optBefore = NULL);
   virtual status_t PingTreeServer(const String & tag, TreeGatewayFlags flags = TreeGatewayFlags());
   virtual status_t PingTreeSeniorPeer(const String & tag, uint32 whichDB = 0, TreeGatewayFlags flags = TreeGatewayFlags());
   virtual status_t RequestTreeNodeSubtrees(const Queue<String> & queryStrings, const Queue<ConstQueryFilterRef> & queryFilters, const String & tag, uint32 maxDepth, TreeGatewayFlags flags = TreeGatewayFlags());
   [... etc ...]

... as well as "hook" methods that your subclass can override in order to get callback-events
from the server, such as:

   virtual void TreeGatewayConnectionStateChanged();
   virtual void TreeNodeUpdated(const String & nodePath, const MessageRef & optPayloadMsg);
   virtual void TreeServerPonged(const String & tag);
   virtual void TreeSeniorPeerPonged(const String & tag, uint32 whichDB);
   virtual void SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData);

For more details about what these methods do, please see the DOxygen comments in ITreeGatewaySubscriber.h.

For a concrete example of how these are used, let's take a look at the FridgeClientCanvas
class (in zg_choir/demos/fridge/client/FridgeClientCanvas.{cpp,h}).  FridgeClientCanvas
is the Qt widget in FridgetClient that actually draws the refrigerator-magnets.  It is
declared like this:

   class FridgeClientCanvas : public QWidget, public ITreeGatewaySubscriber
   {
   public:
      FridgeClientCanvas(ITreeGateway * connector);

      [...]
      virtual void TreeGatewayConnectionStateChanged();
      virtual void TreeNodeUpdated(const String & nodePath, const MessageRef & optPayloadMsg);

      [...]

   private:
      Hashtable<String, MagnetState> _magnets;  // the set of magnets we know about (keyed by node-ID-string)

Note that the only two "hook" callbacks it needs to implement are TreeGatewayConnectionStateChanged()
(which allows it to update its update the GUI when its TCP connection to the server has been
severed or established), and TreeNodeUpdated() (which allows it to update its local-state and GUI in
response to a change to the state of the database on the server).

In FridgeClientCanvas.cpp, we see this:

    FridgeClientCanvas :: FridgeClientCanvas(ITreeGateway * connector)
       : ITreeGatewaySubscriber(connector)
    {
       (void) AddTreeSubscription("magnets/*");
       [...]

Here we see that FridgeClientCanvas is calling AddTreeSubscription() to subscribe to all of
the data-nodes directly under the "magnets" node in the database.  This will result in 
the TreeNodeUpdated() callback being called later on, whenever it is necessary to inform
the FridgetClientCanvas object about updates to the database.

Note that we only have to call AddTreeSubscription() once; in particular we don't need to
call it again after our TCP connection to a server has been disconnected and reconnected.
That is because the ITreeGateway we are registered with will keep track of our subscriptions
here on the client side, and automatically re-transmit them to the new server after a
TCP reconnect has occurred.

The TreeNodeUpdated() callback-method in FridgeClientCanvas is likewise fairly simple:

   void FridgeClientCanvas :: TreeNodeUpdated(const String & nodePath, const MessageRef & optPayloadMsg)
   {
      if (nodePath.StartsWith("magnets/"))
      {
         const String nodeName = nodePath.Substring(8);
         if (optPayloadMsg())
         {
            MagnetState state;
            if ((state.SetFromArchive(*optPayloadMsg()).IsOK())&&(_magnets.Put(nodeName, state).IsOK())) update();
         }
         else if (_magnets.Remove(nodeName).IsOK()) update();
      }
   }

... since the only subscription we made was to "magnets/*", we can reasonably assume that when
TreeNodeUpdated() is called, the path argument supplied will be "magnets/something", where something
is the node-name of the node representing a particular refrigerator-magnet.  But a little paranoia
never hurts, so we double-check that the path is of that form anyway, in the first line.

Once that test is passed, we then extract the node-name from the nodePath string, and check
the (optPayloadMsg) reference.  If it's a NULL reference, that means that the node has been
deleted from the database, so we respond by removing that node from our local _magnets Hashtable
as well, and then calling update() to refresh our GUI.

If OTOH (optPayloadMsg) is not NULL, that means that the specified node has either been added
to the database, or it has been modified with a new payload Message.  In either case, we
respond by using the supplied payload-Message to set the state of a MagnetState object
(via its SetFromArchive(const Message &) method -- see zg_choir/demos/fridget/client/MagnetState.h) 
and then placing that MagnetState object into our _magnets Hashtable, and finally calling update() 
to refresh the GUI.


E. Asking the server to update the database for us

In addition to displaying the current state of the server's database, your client will also
sometimes want to request that the server make changes to the database.  Note that the client
can only make requests; it can't force the server to do anything.  However, a well-programmed
server will generally honor any change-requests if it can do so.

In FridgeClientCanvas, all of the database-change requests are done by calling the method
UploadMagnetState().  It looks like this:

   status_t FridgeClientCanvas :: UploadMagnetState(const String & optNodeID, const MagnetState * optMagnetState)
   {
      MessageRef msgRef;
   
      if (optMagnetState)
      {
         msgRef = GetMessageFromPool();
         if (msgRef() == NULL) RETURN_OUT_OF_MEMORY;
   
         status_t ret;
         if (optMagnetState->SaveToArchive(*msgRef()).IsError(ret)) return ret;
      }
   
      return UploadTreeNodeValue(optNodeID.Prepend("magnets/"), msgRef);
   }

UploadMagnetState() is called from the various Qt mouse-handling functions (mousePressEvent(), mouseMoveEvent(),
etc), and its first argument is the name/ID-string of the database-node we want to change -- or if we are adding a new
refrigerator-magnet and therefore don't have a name/ID-string for its database node, we can pass in an empty
string to indicate that.  (Calling UploadNodeTreeValue() with a path-string that ends in a slash will tell the
server to choose an available node-name/ID-string to use for the new node).

In addition to the (optNodeID) argument, we also pass in a pointer to the MagnetState object we want to upload;
or if we want to delete a node (e.g. because the user has just dragged a magnet out of the window), we can
pass in a NULL pointer here.

If (optMagnetState) is non-NULL, we allocate a Message object, dump the state of the specified MagnetState object
into the Message (via MagnetState::SaveToArchive()), and pass the resulting MessageRef to UploadTreeNodeValue().
Otherwise, we call UploadTreeNodeValue() with a NULL MessageRef(), which tells the server to delete the node
at the specified path.

That's about all there is to FridgeClient.  The rest is boilerplate Qt GUI code, which I won't go into here
because Qt is a separate topic and not the focus of the demo.



I. FridgeServer
---------------

FridgeServer is the server-side counterpart to FridgeClient.  It's wrapped in a GUI application simply to make
it easier to run and manipulate (by those who aren't facile using Terminal), but all the GUI application is
really doing is launching a child process that runs the actual server code and does the ZGChoir server thing.

In real life, your server would likely not have a GUI at all, but instead might run as a background task
on a dedicated server machine.

If you'd like to bypass the FridgeClient GUI and just run the command-line server directly, you can
do so from Terminal, like this (note that the invocation for MacOS/X is shown; the invocation for
Linux or windows would be similar but not the same):

   ./FridgeServer.app/Contents/MacOS/FridgeServer systemname=MyFridge

Supplying the systemname=foo argument tells the code in zg_choir/demos/fridge/server/main.cpp to act
as a command-line process rather than starting up a GUI.

Once the server is running (either from the GUI or from the command line), its tasks are as follows:

   a) Do the ZGChoir database-replication thing, and keep its local database synchronized with
      any other FridgeServer instances on the LAN that share its system-name

   b) Respond to multicast-discovery-pings from FridgeClients, so that interested FridgeClients
      can find it and connect to it via TCP, if they want to

   c) Handle any incoming MUSCLE-Messages coming in via TCP connection from any connected FridgeClients,
      and respond appropriately with the database-update information for nodes that they subscribe to.

Since server processes usually control their own event-loop (rather than having to integrate with
an existing GUI API's event loop), we don't need to supply an ICallbackMechanism for our server
process.  Instead, we can set up a MUSCLE event loop with the necessary components and run it
directly in our thread, as shown in the RunFridgeServerProcess() function in
zg/demos/fridge/server/FridgeServerProcess.cpp.

In particular, we declare various useful objects on the stack at the top of RunFridgeServerProcess():

   CompleteSetupSystem css;                   // Sets up MUSCLE's runtime environment
   FridgePeerSession fridgePeerSession(...);  // responsible for ZGChoir-style database replication
   ZGStdinSession stdinSession(...);          // watches stdin so we can quit if stdin is ever closed
   ServerSideMessageTreeSessionFactory sssFactory(...);  // accepts incoming TCP connections from FridgeClients
   DiscoveryServerSession sdss(...);          // Handles incoming multicast-discovery-pings
   ReflectServer server;                      // Our MUSCLE-based event-loop object

... then we add references to each of those objects to the ReflectServer object, and finally
we call

      ret = server.ServerProcessLoop();

... which runs the server's event loop and lets all of those objects do their jobs.
server.ServerProcessEventLoop() won't return until it is time for the server to go away
(most likely because the ZGStdinSessionObject has noticed that stdin has been closed,
and responded by calling EndServer()).

After ServerProcessLoop() returns, we call server.Cleanup() to unregister all of the
objects we previously added to the ReflectServer, and exit.

So what do all of those objects do?  Some of them (like the CompleteSetupSystem object)
are not very interesting; you just have to add it at the top of main() in order for
MUSCLE to work (it handles annoying details like calling WSAStartup() and WSAShutdown() 
under Windows and that sort of thing).  Others are more interesting, so I'll describe
some of them in more detail below.


A. DiscoveryServerSession

The DiscoveryServerSession object is the server-side counterpart to the SystemDiscoveryClient
that was described back in the FridgeClient section of this document.  DiscoveryServerSession's
job is simply to listen for multicast discovery-ping UDP packets on the ZG discovery 
multicast-group, and (if appropriate) respond to those packets by sending a unicast 
discovery-reply UDP packet back to the querying client.

This is pretty straightforward; the only interesting part is deciding (a) whether to
respond to a query, (b) how quickly to respond, and (c) what data should be included
in the response.

All of these decisions are delegated to an IDiscoveryServerSessionController object, to
which a reference must be specified in the DiscoveryServerSession constructor.  The
IDiscoveryServerSessionController object (which in FridgeServer's case is the
FridgePeerSession) must implement the HandleDiscoveryPing() method, like this:

   uint64 FridgePeerSession :: HandleDiscoveryPing(MessageRef & pingMsg, const IPAddressAndPort & pingSource)
   {
      const uint64 ret = MessageTreeDatabasePeerSession::HandleDiscoveryPing(pingMsg, pingSource);
      if (ret != MUSCLE_TIME_NEVER) (void) pingMsg()->CAddInt16("port", _acceptPort);  // clients will want to know this!
      return ret;
   }

Most of its implementation is delegated up to its superclass -- ZGPeerSession::HandleDiscoveryPing()
decides whether it's appropriate to respond, and what information to place into the reply packet
(by default it places the server's system-name, program-signature, and peer-ID, plus any
user-defined attributes that were specified in the ZGPeerSettings object).

However, there is one other critical piece of information that the client will need to know
and that is the TCP port number that the client will be able to use to connect to this server.
Since the ZGPeerSession class doesn't know about our client-server mechanisms, it can't supply
that information, so that's why we've overridden HandleDiscoveryPing() here to add the TCP
port number to the reply-Message ourself.


B. ServerSideMessageTreeSessionFactory

The ServerSideMessageTreeSessionFactory object's job is to accept incoming TCP connections
on our TCP port, and for each accepted TCP connection, it creates a ServerSideMessageTreeSession
session-object and adds that session-object to the ReflectServer:

   AbstractReflectSessionRef ServerSideMessageTreeSessionFactory :: CreateSession(const String & /*clientAddress*/, const IPAddressAndPort & /*factoryInfo*/)
   {
      ServerSideMessageTreeSessionRef ret(newnothrow ServerSideMessageTreeSession(GetGateway()));
      if (ret() == NULL) WARN_OUT_OF_MEMORY;
      return ret;
   }

... the added ServerSideMessageTreeSession object then becomes that client's "representative"
on the server, and contains code to handle the incoming Message objects from the client,
translate that into the corresponding ITreeGateway method calls as necessary, and send any
resulting reply Message objects back to the client via the TCP connection.  If/when the TCP
connection is closed, its ServerSideMessageTreeSession object will be detached from the
ReflectServer and deleted.


C. FridgePeerSession

The FridgePeerSession is really the heart of the server; it is where all of the ZGChoir-style
database replication happens.  FridgePeerSession is a subclass of MessageTreeDatabasePeerSession,
and MessageTreeDatabasePeerSession is the subclass of ZGPeerSession in which all of the MUSCLE
node-tree (aka MessageTree) functionality has been implemented as a ZGChoir-style database.

Curently, the FridgePeerSession's CreateDatabaseObject(uint32) callback-method is set up
to implement just a single MessageTreeDatabaseObject that handles replication/synchronization
of all of the MUSCLE database-nodes in the system.  That is done for simplicity -- in a more
elaborate system, we could set the CreateDatabaseObject() callback to create and return multiple
IDatabaseObjects and thereby manage multiple databases simultaneously.  These database objects
could refer to different subtrees of the MUSCLE database-node tree, or we could even mix and
match MessageTreeDatabaseObjects with other (non-database-node-tree) subclasses of
IDatabaseObject if we wanted to.


D. ZGStdinSession

This session-object monitors the server-processes stdin file handle.  The main purpose of
this object is to detect when stdin is closed, and respond by causing the server's event-loop
to exit.  That way when the user clicks "Stop Server" in the FridgeServer GUI, the server
sub-process will voluntarily exit immediately, so that the GUI process doesn't have to
forcibly terminate it (which would be rude, and potentially dangerous if the child process
was ever in the middle of something important, like saving a file to disk)

The other thing the ZGStdinSession can do is receive user-supplied text from the parent
process (via stdin), and allow the process to respond intelligently to that text.  That
functionality isn't currently used for much in FridgeServer, but in the future it could
be used to allow the user to interact with the server via the GUI or via Terminal.

The ZGStdinSession class's constructor takes a reference to an ITextCommandReceiver
object, which is an object that has implemented the TextCommandReceived(const String &)
callback-method, which gets called whenever a line of text is received from stdin.

Currently FridgeServer's ZGStdinSession object specified the FridgePeerSession as its
ITextCommandReceiver, and FridgePeerSession::TextCommandReceived(const String &) simply
echoes the text command that was received backed to stdout.













