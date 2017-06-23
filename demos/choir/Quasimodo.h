#ifndef Quasimodo_h
#define Quasimodo_h

#include <QObject>
#include <QAudioOutput>
#include <QIODevice>

#include "ChoirNameSpace.h"
#include "MusicData.h"

namespace choir {

/** This object is in charge of actually ringing the local bells (using QAudioOutput and a mixer algorithm)
  * This is done within a separate thread, so that the timing of the bell-ringing won't be affected GUI operations
  */
class Quasimodo : public QIODevice
{
Q_OBJECT

public:
   /** Constructor */
   Quasimodo(QObject * parent = NULL);

   /** Destructor */
   ~Quasimodo();
   
signals:
   /** Emitted when we've played some bell sounds.  This signal is connected to a slot in
     * the main/GUI thread that will cause the little bell icon to vibrate, so that the user
     * can visually see which bells are "ringing"
     */
   void RangLocalBells(quint64 notesChord);

public slots:
   /** Should be called once at startup, since setting them up in the constructor causes thread-warnings */
   void SetupTheBells();

   /** Tells Quasimodo which bells he should actually play */
   void SetLocalNoteAssignments(quint64 notesChord) {_localNotesChord = notesChord;}

   /** Causes a given bell-sound to be played
     * @param notesChord a bit-chord of CHOIR_NOTE_* values indicating which bell(s) to ring
     * @param localNotesOnly If true, we'll only ring those bells in (notesChord) that are currently assigned to us.
     */
   void RingSomeBells(quint64 notesChord, bool localNotesOnly);

   /** Should be called once at shutdown, since destroying the bells in the destructor causes more thread-warnings */
   void DestroyTheBells();

protected:
   /** Overridden to fill (data) with audio samples from our internal sound-mixing-engine
     * @param data a buffer to fill with audio samples 
     * @param maxSize the number of bytes pointed to by (data)
     * @returns the number of bytes actually written, or -1 on error.
     */
   virtual qint64 readData(char * data, qint64 maxSize);

   /** This method is a no-op and should never be called.  It's only here because QIODevice requires it be implemented. */
   virtual qint64 writeData(const char * /*data*/, qint64 maxSize) {return maxSize;}

private:
   void MixSamples(int16 * mixTo, uint32 numSamplesToMix, uint32 inputSampleOffset, uint64 notesChord) const;

   QAudioOutput * _audioOutput;
   uint64 _localNotesChord;
   uint64 _sampleCounter;

   QByteArray _noteBufs[NUM_CHOIR_NOTES];
   uint32 _maxNoteLengthSamples;

   Hashtable<uint64, uint64> _sampleIndexToNotesChord;
};

}; // end namespace choir

#endif
