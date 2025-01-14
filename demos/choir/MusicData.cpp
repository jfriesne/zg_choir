#include "MusicData.h"
#include "ChoirProtocol.h"  // for CHOIR_NAME_PEER_NICKNAME
#include "ChoirSession.h"

namespace choir {

static const char * _noteNames[NUM_CHOIR_NOTES] = {
   "E6",
   "D6",
   "C6",
   "B5",
   "A5",
   "G5",
   "F5",
   "E5",
   "D5",
   "C5",
   "B4",
   "A4",
   "G4",
   "F4",
   "E4",
   "D4",
   "C4",
   "B3",
   "A3",
   "G3",
   "F3"
};

const char * GetNoteName(uint32 noteIdx)
{
   return (noteIdx < ARRAYITEMS(_noteNames)) ? _noteNames[noteIdx] : "??";
}

String GetPeerNickname(const ZGPeerID & pid, const ConstMessageRef & peerInfo)
{
   char buf[64]; muscleSprintf(buf, "?-" UINT32_FORMAT_SPEC, pid.CalculateChecksum()%1000);
   const String * nickname = peerInfo() ? peerInfo()->GetStringPointer(CHOIR_NAME_PEER_NICKNAME) : NULL;
   if (nickname) return (nickname->Substring(0,8)+(&buf[1]))();
            else return buf;
}

void UpdateNoteHistogram(uint64 chordVal, bool isAdd, Hashtable<uint8, uint32> & histogram, uint64 & bitchord)
{
   for (uint32 i=0; i<(sizeof(uint64)*8); i++)
   {
      if ((chordVal & (1LL<<i)) != 0)
      {
         if (isAdd)
         {
            uint32 * v = histogram.GetOrPut(i);
            if ((v)&&(++(*v) == 1)) bitchord |= (1LL<<i);
         }
         else
         {
            uint32 * v = histogram.Get(i);
            if ((v)&&((--(*v)) == 0))
            {
               (void) histogram.Remove(i);
               bitchord &= ~(1LL<<i);
            }
         }
      }
   }
}

ChoirSession * MusicDatabaseObject :: GetChoirSession()
{
   return static_cast<ChoirSession *>(GetDatabasePeerSession());
}

const ChoirSession * MusicDatabaseObject :: GetChoirSession() const
{
   return static_cast<const ChoirSession *>(GetDatabasePeerSession());
}

ConstMessageRef MusicDatabaseObject :: SendFullStateToGUI(bool allowReviewTrigger)
{
   MessageRef msg = GetMessageFromPool();
   if ((msg() == NULL)||(SaveToArchive(msg).IsError())) return ConstMessageRef();
   SendMessageToGUI(msg, allowReviewTrigger);
   return AddConstToRef(msg);
}

void MusicDatabaseObject :: SendMessageToGUI(const ConstMessageRef & msg, bool allowReviewTrigger)
{
   ChoirSession * cs = GetChoirSession();
   if (cs) cs->SendMessageToGUI(msg, allowReviewTrigger);
}

void MusicDatabaseObject :: SetReviewResults(uint64 allNotesUsedChord)
{
   ChoirSession * cs = GetChoirSession();
   if (cs) cs->SetReviewResults(allNotesUsedChord);
}

}; // end namespace choir
