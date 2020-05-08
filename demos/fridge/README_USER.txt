Note:  This document describes how to compile and run the "FridgeServer"
and "FridgeClient" demo apps.  If you are looking for an explanation
of how the apps work internally, see the README_DEVELOPER.txt file
instead.


I. FridgeClient and FridgeServer description
--------------------------------------------

FridgeClient and FridgeServer are designed to be a very simple demonstration
of a client-server system based on ZGChoir's federated-server technology.

The goal of the demo is to simulate a "refrigerator door", on which the
user can place refrigerator magnets containing random words, and visually
compose interesting sentences.

The user can start up any number of FridgeServers (one per FridgeServer
window).  Each FridgeServer is set to a particular system-name, and will
co-operator with any other FridgeServers that have the same system-name
in the maintenance of the refrigerator-magnet database.  As long as at
least one FridgeServer with a given system-name is still running, that
system's database will remain intact.

FridgeClient is a simple GUI app that allows the user to view and/or
manipulate the refrigerator-magnet database.  When FridgeClient is
launched, it will show the user a list of online fridge-systems that
are available to connect to.  Once the user has chosen a system to
connect to (by double-clicking on it), the user is presented with
a visual display of the magnets in that database.  The user can add
a new magnet by clicking on the window's background, or move existing
magnets via click-and-drag.  It's also possible to delete a magnet
by dragging it off the edge of the window, or clear all the magnets
from the database to start over from scratch.  Any actions taken
by the user in one FridgeClient window will be immediately visible
to all FridgeClients that are connected to the same fridge-system
(even if they are not connected to the same FridgeServer).


II. Building FridgeClient and FridgeServer
------------------------------------------

In order to build FridgeClient and FridgeServer, you must
have a C++ compiler and a recent (5.x) version of Qt installed.
You can check that Qt is installed by opening a Terminal/shell 
window and typing qmake -- if qmake prints a help text, you
are good to go; OTOH if you get a "command not found" error,
then Qt is either not installed, or $QTDIR/bin is not in your
PATH variable, and you'll need to do some additional setup
before you can build.

To build FridgeServer:

	cd zg_choir/demos/fridge/server/
	qmake
	make -j4

... when the build completes, you should have a FridgeServer 
executable in your zg_choir/demos/fridge/server directory that
you can run (either by double-clicking its icon or from the
shell).

To build FridgeClient:

	cd zg_choir/demos/fridge/client/
	qmake
	make -j4

... when the build completes, you should have a FridgeClient 
executable in your zg_choir/demos/fridge/client directory that
you can run (either by double-clicking its icon or from the
shell).


III. Things to see and do
-------------------------

You can test FridgeClient and FridgeServer's basic functionality
using a single FridgeServer window and a single FridgeClient window,
but to get a better feel for what's possible, I recommend making
use of the "Clone Window" button in both the FridgeServer and
FridgeClient GUIs.

The "Clone Window" will create an additional server (or client)
window for you to use.  This is logically equivalent to hooking
up a separate computer to the LAN and running another copy of
FridgeClient (or FridgeServer) there, but a bit more convenient
to do since it doesn't require hunting down additional computers
and Ethernet cables.

When you have multiple FridgeClient windows open, and they are
connected to the same fridge-system, they should all display the
same magnets at all times -- that demonstrates the ability of
ZGChoir to keep all connected clients synchronized to the state
of the database.

When you have multiple FridgeServer windows open, your refrigerator-magnets
database should remain usable as long as at least one of the FridgeServers
remains running.  To test this, you can click "Stop Server" on each of
the FridgeServer window in sequence.  When you stop the server that a
FridgeClient window is currently connected to, you should see the FridgeClient
window flicker briefly, as the FridgeClient detects the loss of the TCP
connection and respond by auto-reconnecting to another FridgeServe in
the same fridge-system.  Beware that if you "Stop Server" on all of 
the FridgeServers in the same system simultaneously, you will lose the
state of the magnets-database for that system (since there are no 
servers left to hold a copy of it) -- so you may want to re-start some
of your stopped FridgeServers before you click "Stop Server" on the last
one in the system.

When a FridgeServer window is in its "stopped" state (i.e. its server
sub-process is not running), it is possible to edit the text in the
System Name field.  By changing it to a different System Name (any
name will do) and then re-starting the server, you are placing that
FridgeServer into a different fridge-system, and thus it will hold
a different database that is independent of the database associated
with the original System Name.  Doing this will add the new System
Name as an option in the Discovered Systems List that the FridgeClient
displays at startup.


