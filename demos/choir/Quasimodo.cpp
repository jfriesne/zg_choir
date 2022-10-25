#include <QAudioFormat>
#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
# include <QAudioDevice>
# include <QMediaDevices>
#endif
#include <QFile>

#include "Quasimodo.h"

namespace choir {

static const int AUDIO_SAMPLE_RATE       = 22050;
static const int MIX_BUFFER_SIZE_SAMPLES = AUDIO_SAMPLE_RATE/20;  // 20Hz buffers, for now

// Stolen from:  https://stackoverflow.com/questions/13660777/c-reading-the-data-part-of-a-wav-file
struct WavHeader 
{
   uint8  RIFF[4];        // RIFF Header (Magic number)
   uint32 ChunkSize;      // RIFF Chunk Size  
   uint8  WAVE[4];        // WAVE Header      
   uint8  fmt[4];         // FMT header       
   uint32 Subchunk1Size;  // Size of the fmt chunk                                
   uint16 AudioFormat;    // Audio format 1=PCM,6=mulaw,7=alaw, 257=IBM Mu-Law, 258=IBM A-Law, 259=ADPCM 
   uint16 NumOfChan;      // Number of channels 1=Mono 2=Sterio                   
   uint32 SamplesPerSec;  // Sampling Frequency in Hz                             
   uint32 bytesPerSec;    // bytes per second 
   uint16 blockAlign;     // 2=16-bit mono, 4=16-bit stereo 
   uint16 bitsPerSample;  // Number of bits per sample      
   uint8  Subchunk2ID[4]; // "data"  string   
   uint32 Subchunk2Size;  // Sampled data length    
}; 

QByteArray ReadWaveFileDataFromResource(const QString & waveFilePath)
{
   QFile file(waveFilePath);
   if (file.open(QIODevice::ReadOnly))
   {
      WavHeader header;
      if (file.read((char *)&header, sizeof(header)) == sizeof(header))
      {
         if ((header.RIFF[0] == 'R')&&(header.RIFF[1] == 'I')&&(header.RIFF[2] == 'F')&&(header.RIFF[3] == 'F')&&
             (header.AudioFormat == 1)&&(header.NumOfChan == 1)&&(header.SamplesPerSec == AUDIO_SAMPLE_RATE)&&(header.blockAlign == 2)&&(header.bitsPerSample == 16))
         {
            const uint32 numIndicatedSamples = (uint32)(header.Subchunk2Size            / sizeof(int16));
            const uint32 numActualSamples    = (uint32)((file.size()-sizeof(WavHeader)) / sizeof(int16));
            if (numIndicatedSamples != numActualSamples) LogTime(MUSCLE_LOG_WARNING, "File indicates " UINT32_FORMAT_SPEC " samples but I see " UINT32_FORMAT_SPEC "\n", numIndicatedSamples, numActualSamples);
            const uint32 numSamplesToLoad = muscleMin(numIndicatedSamples, numActualSamples);
            const uint32 numBytesToLoad   = numSamplesToLoad*sizeof(int16);
            QByteArray ret(numBytesToLoad, 0);
            if (file.read(ret.data(), numBytesToLoad) == numBytesToLoad) 
            {
               int16 * buf = (int16 *) ret.data();
               for (uint32 i=0; i<numSamplesToLoad; i++) buf[i] = LittleEndianConverter::Import<int16>(&buf[i])/2;  // yes, always little-endian, because WAV!
               return ret;
            }
            else LogTime(MUSCLE_LOG_ERROR, "Unable to read " UINT32_FORMAT_SPEC " bytes of audio data from [%s]\n", numBytesToLoad,  waveFilePath.toUtf8().constData());
         }
         else LogTime(MUSCLE_LOG_ERROR, "Bad WAV header at [%s]\n", waveFilePath.toUtf8().constData());  
      }
      else LogTime(MUSCLE_LOG_ERROR, "Unable to read WAV header at [%s]\n", waveFilePath.toUtf8().constData());  
   }
   else LogTime(MUSCLE_LOG_ERROR, "Unable to open WAV file at [%s]\n", waveFilePath.toUtf8().constData());  

   return QByteArray();
}

Quasimodo :: Quasimodo(QObject * parent) : QIODevice(parent), _audioOutput(NULL), _localNotesChord(0), _sampleCounter(0), _maxNoteLengthSamples(0)
{
   setOpenMode(QIODevice::ReadOnly);
}

Quasimodo :: ~Quasimodo()
{
   // empty
}
   
void Quasimodo :: SetupTheBells()
{
   QAudioFormat fmt;
   fmt.setChannelCount(1);
   fmt.setSampleRate(AUDIO_SAMPLE_RATE);
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
   fmt.setSampleSize(16);
   fmt.setSampleType(QAudioFormat::SignedInt);
   fmt.setByteOrder(QAudioFormat::LittleEndian);
   fmt.setCodec("audio/pcm");
#else
   fmt.setSampleFormat(QAudioFormat::Int16);
#endif

   for (uint32 i=0; i<ARRAYITEMS(_noteBufs); i++) 
   {
      _noteBufs[i] = ReadWaveFileDataFromResource(QString(":/bell_%1.wav").arg(i));
      _maxNoteLengthSamples = muscleMax(_maxNoteLengthSamples, (uint32) (_noteBufs[i].size()/sizeof(int16)));
   }

#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
   QAudioDeviceInfo adi(QAudioDeviceInfo::defaultOutputDevice());
#else
   QAudioDevice adi(QMediaDevices::defaultAudioOutput());
#endif
   if (adi.isFormatSupported(fmt) == false) LogTime(MUSCLE_LOG_ERROR, "Audio format not supported by audio device, can't play audio!\n");

#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
   _audioOutput = new QAudioOutput(adi, fmt);
#else
   _audioOutput = new QAudioSink(adi, fmt);
#endif

   _audioOutput->setBufferSize(MIX_BUFFER_SIZE_SAMPLES*sizeof(int16));  // for lower latency
   _audioOutput->start(this);
}

void Quasimodo :: MixSamples(int16 * mixTo, uint32 numSamplesToMix, uint32 inputSampleOffset, uint64 notesChord) const
{
   for (uint32 i=0; i<NUM_CHOIR_NOTES; i++)
   {
      if ((notesChord & (1LL<<i)) != 0)
      {
         const uint32 noteBufSizeSamples = _noteBufs[i].size()/sizeof(int16);
         if (inputSampleOffset < noteBufSizeSamples)
         {
            const uint32 numSamplesToMixForThisNote = muscleMin((uint32)(noteBufSizeSamples-inputSampleOffset), numSamplesToMix);

            const int16 * noteBuf = ((const int16 *) _noteBufs[i].constData())+inputSampleOffset;
            for (uint32 j=0; j<numSamplesToMixForThisNote; j++) mixTo[j] += noteBuf[j];
         }
      }
   }
}

qint64 Quasimodo :: readData(char * data, qint64 maxSize)
{
   maxSize = muscleMin(maxSize, (qint64) (MIX_BUFFER_SIZE_SAMPLES*sizeof(int16)));  // to keep our latency low

   const uint32 maxNumSamples = (uint32) (maxSize/sizeof(int16));
   int16 * outBuf = (int16 *) data;
   memset(outBuf, 0, maxNumSamples*sizeof(int16));

   for (HashtableIterator<uint64, uint64> iter(_sampleIndexToNotesChord); iter.HasData(); iter++)
   {
      const uint64 sampleIndex = iter.GetKey();
      const uint64 notesChord  = iter.GetValue();

      const int64 offsetAfterTopOfChord = _sampleCounter-sampleIndex;  // how far after the first sample we're at
      if (offsetAfterTopOfChord >= _maxNoteLengthSamples) 
      {
         // No more mixing to do for this chord, ever, so we can get rid of it
         (void) _sampleIndexToNotesChord.Remove(sampleIndex);
      }
      else if (offsetAfterTopOfChord < 0)
      {
         const int64 offsetUntilTopOfChord = -offsetAfterTopOfChord;
         if (offsetUntilTopOfChord < maxNumSamples)
         {
            // skip past some empty/silent space, then mix some audio
            MixSamples(outBuf+offsetUntilTopOfChord, maxNumSamples-offsetUntilTopOfChord, 0, notesChord);
         }
         else break;  // nothing to mix yet, it's all too far in the future(!)
      }
      else
      {
         // We're starting in the middle (or at the beginning) of the samples
         MixSamples(outBuf, maxNumSamples, (uint32)offsetAfterTopOfChord, notesChord);
      }
   }
   
   _sampleCounter += maxNumSamples;
   return maxNumSamples*sizeof(int16);
}

void Quasimodo :: DestroyTheBells()
{
   _audioOutput->stop();
   delete _audioOutput;
   _audioOutput = NULL;

   for (uint32 i=0; i<ARRAYITEMS(_noteBufs); i++) _noteBufs[i].clear();
   _maxNoteLengthSamples = 0;
}

void Quasimodo :: RingSomeBells(quint64 notesChord, bool localNotesOnly)
{
   if (localNotesOnly) notesChord &= _localNotesChord;
   if (notesChord == 0) return;

   uint64 * nc = _sampleIndexToNotesChord.GetOrPut(_sampleCounter);
   if (nc)
   {
      emit RangLocalBells(notesChord & ~(*nc));
      *nc |= notesChord;
   }
}

}; // end namespace choir
