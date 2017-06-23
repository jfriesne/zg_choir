#include "ChoirProtocol.h"

namespace choir {

// These strings are short only because I'm overly ambitious about saving network bandwidth; they could really be any length --jaf
const String CHOIR_NAME_CHORD_INDEX      = "c_i";
const String CHOIR_NAME_CHORD_VALUE      = "c_v";
const String CHOIR_NAME_NOTE_INDEX       = "n_i";
const String CHOIR_NAME_WRAPPED_MESSAGE  = "w_m";
const String CHOIR_NAME_LOOP             = "loo";
const String CHOIR_NAME_MICROS_PER_CHORD = "upc";
const String CHOIR_NAME_SONG_FILE_PATH   = "sfp";
const String CHOIR_NAME_PEER_NICKNAME    = "nme";
const String CHOIR_NAME_PEER_ID          = "pid";
const String CHOIR_NAME_PEER_INFO        = "pin";
const String CHOIR_NAME_PEER_LATENCY     = "lat";
const String CHOIR_NAME_STRATEGY         = "str";

}; // end namespace choir
