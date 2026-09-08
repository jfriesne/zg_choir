Note:  This document describes how to compile and run the "ZGBrowser"
demo app.


I. ZGBrowser description
------------------------

ZGBrowser is a Qt GUI app for browsing the databases of the ZGChoir
systems that are running on the local network.

It is the ZG-flavoured counterpart of MUSCLE's qt_muscled_browser demo
(which browses a single muscled server, rather than a ZG system).  Both
show the same sort of thing:  a live, subscription-driven view of a
server's node tree.

When ZGBrowser is launched, it shows the user a list of the ZG systems
that it can see on the LAN, with each system's signature, peer count and
IP addresses.  Systems appear in and disappear from this list on their
own as servers come and go.  The user can also type in a system name
(wildcards allowed) to connect to a system directly.

Once the user has chosen a system to connect to (by double-clicking on
it), the user is presented with a tree view of that system's database.
A node is subscribed to when the user opens it, and unsubscribed from
when the user closes it, so that the client only ever holds the part of
the database that is actually on screen -- and that part is always live.
Nodes that are added, changed or deleted on the server show up in the
tree immediately.

Selecting a node dumps that node's Message payload into the panel on
the right (inflating the Message first, if it happens to be stored in
zlib-deflated form).

If the system goes away, an overlay says so, and the connection is
retried automatically.  When the system comes back, the tree rebuilds
itself and the nodes that the user had open are re-opened.


II. Building ZGBrowser
----------------------

In order to build ZGBrowser, you must have a C++17 compiler and a recent
version of Qt (Qt 5 or Qt 6) installed.  ZGBrowser can be built either
with qmake or with CMake; use whichever you prefer.

To build with qmake:

	cd zg_choir/demos/zg_browser/
	qmake
	make -j4

You can check that Qt is installed by opening a Terminal/shell window
and typing qmake -- if qmake prints a help text, you are good to go;
OTOH if you get a "command not found" error, then Qt is either not
installed, or $QTDIR/bin is not in your PATH variable, and you'll need
to do some additional setup before you can build.

... when the build completes, you should have a ZGBrowser executable in
your zg_choir/demos/zg_browser directory that you can run (either by
double-clicking its icon or from the shell).

To build with CMake instead, ZGBrowser is built as part of the zg_choir
CMake project, together with the other Qt-based demo apps, by enabling
the WITH_DEMOS option:

	cd zg_choir
	cmake -B build -DWITH_DEMOS=ON
	cmake --build build -j4

If CMake can't find your Qt installation, tell it where to look by
adding -DCMAKE_PREFIX_PATH=/path/to/your/Qt (e.g. on MacOS/M1,
-DCMAKE_PREFIX_PATH=/Applications/Qt/6.10.1/macos).  The CMake build
places the executable in zg_choir/build/demos/zg_browser instead.

Note that on MacOS, either build produces an application bundle, so the
executable itself lives inside it, at ZGBrowser.app/Contents/MacOS/ZGBrowser.

ZGBrowser accepts the following command-line options:

	-s, --system-name <name>   Skip the systems list and browse this
	                           system straight away (wildcards allowed).
	-h, --help                 Print usage text.


III. Things to see and do
-------------------------

There is nothing to browse unless a ZG server is running somewhere on
the LAN, so the easiest way to try ZGBrowser out is to run zg_choir's
own tree_server test program.  It is built by the CMake build described
above (the qmake build only builds ZGBrowser itself), and ends up in
your zg_choir/build directory:

	cd zg_choir
	cmake -B build && cmake --build build -j4    # if you haven't already
	./build/tree_server          # advertises the system "test_tree_system"

Run it (in as many copies as you like -- they will form a single
multi-peer system) and it will show up in ZGBrowser's systems list.

To put some data in the database for ZGBrowser to show, run zg_choir's
tree_client test program in another shell window:

	./build/tree_client

... and type commands like "s srv/machines/alpha" at its prompt to set
nodes, or "d srv/machines/alpha" to delete them.  You should see the
nodes appear in and disappear from the ZGBrowser window as you type.

You can also run several ZGBrowser windows at once, connected to the
same system; they should all display the same tree at all times -- that
demonstrates the ability of ZGChoir to keep all connected clients
synchronized to the state of the database.

Note that on MacOS 15 and later, the first run of ZGBrowser will raise
a local-network permission prompt.  ZG's discovery mechanism uses
multicast, so if you decline that prompt, the systems list will remain
empty.
