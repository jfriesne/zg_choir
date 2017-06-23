#ifndef ChoirProtocol_h
#define ChoirProtocol_h

#include "util/String.h"
#include "ChoirNameSpace.h"

namespace choir {

#define CHOIR_VERSION_STRING "0.8b"  /**< The user-visible version string for the ZGChoir application */

enum {
   CHOIR_DATABASE_SCORE = 0,     // the music itself
   CHOIR_DATABASE_PLAYBACKSTATE, // what and where we are currently playing at
   CHOIR_DATABASE_ROSTER,        // names and other info about our bell-ringers
   NUM_CHOIR_DATABASES
};

enum {
   // commands for updating the CHOIR_DATABASE_SCORE database
   CHOIR_COMMAND_TOGGLE_NOTE = 1668245874, // 'coir' 
   CHOIR_COMMAND_SET_SONG_FILE_PATH, 
   CHOIR_COMMAND_SET_CHORD,  // sent by senior peer to junior peers
   CHOIR_COMMAND_INSERT_CHORD,  // inserts a column at the given index; all notes afterwards move to the right
   CHOIR_COMMAND_DELETE_CHORD,  // deletes a column at the given index; all notes afterwards move to the left

   // commands for updating the CHOIR_DATABASE_PLAYBACKSTATE database
   CHOIR_COMMAND_PLAY,       // may include a seek-command
   CHOIR_COMMAND_PAUSE,      // may include a seek-command
   CHOIR_COMMAND_ADJUST_PLAYBACK,

   // commands for updating the CHOIR_DATABASE_ROSTER database
   CHOIR_COMMAND_TOGGLE_ASSIGNMENT,
   CHOIR_COMMAND_UNASSIGN_ORPHANS,
   CHOIR_COMMAND_REVIEW_ASSIGNMENTS,  // if we have an automated bell-assignment strategy, here's where we implement it
   CHOIR_COMMAND_SET_STRATEGY,
   CHOIR_COMMAND_NOOP,

   // Commands sent from local ChoirSession to GUI
   CHOIR_COMMAND_PEER_ONLINE,
   CHOIR_COMMAND_PEER_OFFLINE,
};

enum {
   CHOIR_REPLY_GUI_UPDATE = 1668444268, // 'crpl'   // send from our local network thread back to the GUI
   CHOIR_REPLY_LATENCIES_TABLE,                     // new estimated-network-latencies to display
   CHOIR_REPLY_NEW_SENIOR_PEER
};

extern const String CHOIR_NAME_CHORD_INDEX;
extern const String CHOIR_NAME_CHORD_VALUE;
extern const String CHOIR_NAME_NOTE_INDEX;
extern const String CHOIR_NAME_WRAPPED_MESSAGE;
extern const String CHOIR_NAME_MICROS_PER_CHORD;
extern const String CHOIR_NAME_LOOP;
extern const String CHOIR_NAME_SONG_FILE_PATH;
extern const String CHOIR_NAME_PEER_NICKNAME;
extern const String CHOIR_NAME_PEER_ID;
extern const String CHOIR_NAME_PEER_INFO;
extern const String CHOIR_NAME_PEER_LATENCY;
extern const String CHOIR_NAME_STRATEGY;

}; // end namespace choir

#endif
