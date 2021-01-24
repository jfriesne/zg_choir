#include "reflector/ReflectServer.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"
#include "util/StringTokenizer.h"

#include "zg/ZGConstants.h"  // for GetRandomNumber()
#include "zg/ZGPeerSession.h"
#include "zg/ZGStdinSession.h"

using namespace zg;

enum {
   TOY_DB_COMMAND_SET_DB_STATE = 1953462628, // 'toyd' -- clears the current DB state and then adds the specified key/value pairs
   TOY_DB_COMMAND_PUT_STRINGS,               //        -- adds the specified key/value pairs (overwriting the value of any existing keys that match the new keys)
   TOY_DB_COMMAND_REMOVE_STRINGS,            //        -- removes any key/value pairs whose keys match those found in the Message
   TOY_DB_COMMAND_USER_TEXT,                 //        -- just some chat text to print when received, for testing
};

enum {NUM_TOY_DATABASES = 1};  // for now!

static ZGPeerSettings GetTestZGPeerSettings(const Message & args)
{
   // Just so we can see that this is working
   MessageRef peerAttributes = GetMessageFromPool();
   peerAttributes()->AddString("testing", "attributes");
   peerAttributes()->AddInt32("some_value", (GetRunTime64()%10000));
   peerAttributes()->AddFloat("pi", 3.14159f);

   ZGPeerSettings s("test_peer", "test_system", NUM_TOY_DATABASES, false);
   s.SetPeerAttributes(peerAttributes);

   String multicastMode;
   if (args.FindString("multicast", multicastMode) == B_NO_ERROR)
   {
      if (multicastMode.ContainsIgnoreCase("sim"))
      {
         LogTime(MUSCLE_LOG_INFO, "Forcing all network interfaces to use SimulatedMulticastDataIO!\n");
         s.SetMulticastBehavior(ZG_MULTICAST_BEHAVIOR_SIMULATED_ONLY);      
      }
      else if (multicastMode.ContainsIgnoreCase("standard"))
      {
         LogTime(MUSCLE_LOG_INFO, "Forcing all network interfaces to use real multicast DataIO!\n");
         s.SetMulticastBehavior(ZG_MULTICAST_BEHAVIOR_STANDARD_ONLY);      
      }
   }

   String maxLogSizeBytesStr;
   if (args.FindString("maxlogsizebytes", maxLogSizeBytesStr) == B_NO_ERROR)
   {
      uint32 maxBytes = atol(maxLogSizeBytesStr());
      if (maxBytes > 0)
      {
         LogTime(MUSCLE_LOG_INFO, "Setting maximum log size for database #0 to " UINT32_FORMAT_SPEC " bytes.\n", maxBytes);
         s.SetMaximumUpdateLogSizeForDatabase(0, maxBytes);
      }
      else LogTime(MUSCLE_LOG_WARNING, "maxlogsizebytes argument didn't contain a value greater than zero, ignoring it.\n");
   }

   return s;
}

// This class implements a simple ZGPeer for testing/validation purposes
class TestZGPeerSession : public ZGPeerSession
{
public:
   TestZGPeerSession(const Message & args) 
      : ZGPeerSession(GetTestZGPeerSettings(args))
      , _seed((unsigned int) time(NULL))
      , _autoUpdateDelay(0)
      , _nextAutoUpdateTime(MUSCLE_TIME_NEVER)
      , _nextPrintNetworkTimeTime(MUSCLE_TIME_NEVER)
      , _prevNetworkTime(0)
      , _prevLocalTime(0)
      , _printDBPending(false)
   {/* empty */}

   virtual const char * GetTypeName() const {return "TestZGPeer";}

   virtual bool TextCommandReceived(const String & text)
   {
      if (text.Equals("reset"))
      {
         if (RequestResetDatabaseStateToDefault(0) == B_NO_ERROR) LogTime(MUSCLE_LOG_INFO, "Database reset-request sent!\n");
                                                             else LogTime(MUSCLE_LOG_INFO, "Database reset-request failed!\n");
      }
      else if (text.StartsWith("replace all "))
      {
         MessageRef newStateMsg = ParseTestInstructions(TOY_DB_COMMAND_SET_DB_STATE, text.WithoutPrefix("replace all "));
         const String s = TestInstructionsToString(newStateMsg);
         if (RequestReplaceDatabaseState(0, newStateMsg) == B_NO_ERROR) LogTime(MUSCLE_LOG_INFO,  "Database replace-all-request [%s] sent\n", s());
                                                                   else LogTime(MUSCLE_LOG_ERROR, "Database replace-all-request [%s] failed!\n", s());
      }
      else if ((text.StartsWith("del "))||(text.StartsWith("rm "))||(text.StartsWith("delete "))||(text.StartsWith("remove ")))
      {
         int32 firstSpace = text.IndexOf(' ');
         MessageRef updateMsg = ParseTestInstructions(TOY_DB_COMMAND_REMOVE_STRINGS, text.Substring(firstSpace));
         const String s = TestInstructionsToString(updateMsg);
         if (RequestUpdateDatabaseState(0, updateMsg) == B_NO_ERROR) LogTime(MUSCLE_LOG_INFO,  "Database delete-items-request [%s] sent\n", s());
                                                                else LogTime(MUSCLE_LOG_ERROR, "Database delete-items-request [%s] failed!\n", s());
      }
      else if ((text.StartsWith("put "))||(text.StartsWith("set "))||(text.Contains('=')))
      {
         MessageRef updateMsg = ParseTestInstructions(TOY_DB_COMMAND_PUT_STRINGS, text.WithoutPrefix("put ").WithoutPrefix("set "));
         const String s = TestInstructionsToString(updateMsg);
         if (RequestUpdateDatabaseState(0, updateMsg) == B_NO_ERROR) LogTime(MUSCLE_LOG_INFO,  "Database put-items-request [%s] sent\n", s());
                                                                else LogTime(MUSCLE_LOG_ERROR, "Database put-items-request [%s] failed!\n", s());
      }
      else if (text.StartsWith("sendunicast"))
      {
         StringTokenizer tok(text.Substring(12).Trim()(), NULL);
         const char * target = tok();
         if (target)
         {
            const char * chatText = tok.GetRemainderOfString();

            MessageRef msg = GetMessageFromPool(TOY_DB_COMMAND_USER_TEXT);
            if ((msg())&&(msg()->CAddString("chat_text", chatText) == B_NO_ERROR))
            {
               if (strcmp(target, "*") == 0)
               {
                  LogTime(MUSCLE_LOG_INFO, "Sending chat text [%s] to all peers via unicast.\n", chatText);
                  SendUnicastUserMessageToAllPeers(msg);
               }
               else
               {
                  ZGPeerID targetPeerID;
                  targetPeerID.FromString(target);
                  if (targetPeerID.IsValid())
                  {
                     LogTime(MUSCLE_LOG_INFO, "Sending chat text [%s] to peer [%s] via unicast.\n", chatText, targetPeerID.ToString()());
                     SendUnicastUserMessageToPeer(targetPeerID, msg);
                  }
                  else LogTime(MUSCLE_LOG_INFO, "Unable to parse target peer ID [%s]\n", target);
               }
            }
         }
         else LogTime(MUSCLE_LOG_ERROR, "Usage:  sendunicast <peerID> [msg text]\n");
      }
      else if (text.StartsWith("sendmulticast"))
      {
         const String chatText = text.Substring(14).Trim();

         MessageRef msg = GetMessageFromPool(TOY_DB_COMMAND_USER_TEXT);
         if ((msg())&&(msg()->CAddString("chat_text", text.Substring(14).Trim()) == B_NO_ERROR))
         {
            LogTime(MUSCLE_LOG_INFO, "Sending chat text [%s] to all peers via multicast.\n", chatText());
            SendMulticastUserMessageToAllPeers(msg);
         }
      }
      else if (text == "print db")
      {
         SchedulePrintDB();
      }
      else if (text == "print log")
      {
         PrintDatabaseUpdateLog();
      }
      else if (text.StartsWith("timer"))
      {
         uint64 updateTimeMicros = (text.Length() > 6) ? MillisToMicros(atol(text()+6)) : 0;
         if (updateTimeMicros > 0) LogTime(MUSCLE_LOG_INFO, "Setting auto-update timer to [%s]\n", GetHumanReadableTimeIntervalString(updateTimeMicros)());
                              else LogTime(MUSCLE_LOG_INFO, "Disabling auto-update-timer\n");
         _autoUpdateDelay = updateTimeMicros;
         _nextAutoUpdateTime = (updateTimeMicros==0)?MUSCLE_TIME_NEVER:GetRunTime64();
         InvalidatePulseTime();
      }
      else if (text == "start network times") {_nextPrintNetworkTimeTime = GetRunTime64();    InvalidatePulseTime();}
      else if (text == "stop network times")  {_nextPrintNetworkTimeTime = MUSCLE_TIME_NEVER; InvalidatePulseTime(); _prevNetworkTime = _prevLocalTime = 0;}
      else return ZGPeerSession::TextCommandReceived(text); 

      return true;  // if we got here, one of our if-cases must have been executed
   }

protected:
   virtual ConstMessageRef SeniorUpdateLocalDatabase(uint32 whichDatabase, uint32 & dbChecksum, const ConstMessageRef & seniorDoMsg)
   {
      SchedulePrintDB();
      return (HandleUpdate(seniorDoMsg()->what, whichDatabase, dbChecksum, seniorDoMsg) == B_NO_ERROR) ? seniorDoMsg : ConstMessageRef();
   }

   virtual status_t JuniorUpdateLocalDatabase(uint32 whichDatabase, uint32 & dbChecksum, const ConstMessageRef & juniorDoMsg)
   {
      SchedulePrintDB();
      return HandleUpdate(juniorDoMsg()->what, whichDatabase, dbChecksum, juniorDoMsg);
   }

   virtual void ResetLocalDatabaseToDefault(uint32 whichDatabase, uint32 & dbChecksum)
   {
      SchedulePrintDB();
      _toyDatabases[whichDatabase].Clear();
      dbChecksum = 0;
   }

   virtual MessageRef SaveLocalDatabaseToMessage(uint32 whichDatabase) const
   {
      MessageRef ret = GetMessageFromPool(TOY_DB_COMMAND_SET_DB_STATE);
      if (ret()==NULL) return MessageRef();

      for (HashtableIterator<String, String> iter(_toyDatabases[whichDatabase]); iter.HasData(); iter++)
         if (ret()->AddString(iter.GetKey(), iter.GetValue()) != B_NO_ERROR) return MessageRef();
     
      return ret;
   }

   virtual status_t SetLocalDatabaseFromMessage(uint32 whichDatabase, uint32 & dbChecksum, const ConstMessageRef & newDBStateMsg)
   {
      SchedulePrintDB();
      if (newDBStateMsg()->what != TOY_DB_COMMAND_SET_DB_STATE) return B_ERROR;

      ResetLocalDatabaseToDefault(whichDatabase, dbChecksum);
      return HandleUpdate(TOY_DB_COMMAND_PUT_STRINGS, whichDatabase, dbChecksum, newDBStateMsg);
   }

   virtual uint32 CalculateLocalDatabaseChecksum(uint32 whichDatabase) const
   {
      uint32 ret = 0;
      for (HashtableIterator<String, String> iter(_toyDatabases[whichDatabase]); iter.HasData(); iter++) ret += CalculateKeyValueChecksum(iter.GetKey(), iter.GetValue());
      return ret;
   }

   virtual String GetLocalDatabaseContentsAsString(uint32 whichDatabase) const
   {
      String ret;
      for (HashtableIterator<String, String> iter(_toyDatabases[whichDatabase]); iter.HasData(); iter++) ret += String("   [%1] -> [%2]\n").Arg(iter.GetKey()).Arg(iter.GetValue()());
      return ret;
   }

   virtual void MessageReceivedFromPeer(const ZGPeerID & fromPeerID, const MessageRef & msg)
   {
      printf("Received incoming Message from peer [%s]:\n", fromPeerID.ToString()());
      msg()->PrintToStream();
   }

   virtual uint64 GetPulseTime(const PulseArgs & args) {return _printDBPending ? 0 : muscleMin(ZGPeerSession::GetPulseTime(args), _nextAutoUpdateTime, _nextPrintNetworkTimeTime);}

   virtual void Pulse(const PulseArgs & args)
   {
      ZGPeerSession::Pulse(args);
      if (args.GetScheduledTime() >= _nextAutoUpdateTime)
      {
         _nextAutoUpdateTime = args.GetScheduledTime() + _autoUpdateDelay;
         SendRandomDatabaseUpdateRequest();
      }

      if (args.GetScheduledTime() >= _nextPrintNetworkTimeTime)
      {
         // Print out some debug info so we can see if our network-time-clock is behaving itself properly
         const uint64 localTime   = GetRunTime64();
         const uint64 networkTime = GetNetworkTime64();
         const int64 drift        = (_prevNetworkTime == 0) ? 0 : ((networkTime-_prevNetworkTime) - (localTime-_prevLocalTime));

         printf("Network time is now " UINT64_FORMAT_SPEC " (%s since previous print, %s drift)\n", networkTime, GetHumanReadableSignedTimeIntervalString((_prevNetworkTime==0)?0:(networkTime-_prevNetworkTime), 1)(), GetHumanReadableSignedTimeIntervalString(drift, 1)());

         _prevNetworkTime          = networkTime;
         _prevLocalTime            = localTime;
         _nextPrintNetworkTimeTime = localTime + MillisToMicros(500);
      }

      if (_printDBPending)
      {
         _printDBPending = false;
         PrintDB();
      }
   }

private:
   void SchedulePrintDB()
   {
      if (_printDBPending == false)
      {
         _printDBPending = true;
         InvalidatePulseTime();
      }
   }

   void PrintDB() const
   {
      printf("\n----------- DATABASE STATE ------------\n");
      PrintDatabaseStateInfo();
      for (HashtableIterator<String, String> iter(_toyDatabases[0]); iter.HasData(); iter++) printf("   [%s] -> [%s]\n", iter.GetKey()(), iter.GetValue()());
   }

   MessageRef ParseTestInstructions(uint32 what, const String & s) const
   {
      MessageRef m = GetMessageFromPool(what);
      if ((m() == NULL)||(ParseArgs(s, *m(), true) != B_NO_ERROR)) return MessageRef();
      return m;
   }

   String TestInstructionsToString(const MessageRef & msg) const
   {
      String ret = msg() ? UnparseArgs(*msg()) : "???";
      ret.Replace('\n', ' ');  // we want a single-line output, please
      return ret;
   }

   status_t HandleUpdate(uint32 cmdWhat, uint32 whichDatabase, uint32 & dbChecksum, const ConstMessageRef & cmdMsg)
   {
      Hashtable<String, String> & toyDB = _toyDatabases[whichDatabase];

      switch(cmdWhat)
      {
         case TOY_DB_COMMAND_SET_DB_STATE:
            return SetLocalDatabaseFromMessage(whichDatabase, dbChecksum, cmdMsg);

         case TOY_DB_COMMAND_PUT_STRINGS:
         {
            for (MessageFieldNameIterator fnIter(*cmdMsg(), B_STRING_TYPE); fnIter.HasData(); fnIter++)
            {
               const String & keyStr = fnIter.GetFieldName();
               const String & valStr = *cmdMsg()->GetStringPointer(keyStr);

               String * oldValue = toyDB.Get(keyStr);
               if (oldValue)
               {
                  // overwrite-existing key-value pair
                  dbChecksum -= CalculateKeyValueChecksum(keyStr, *oldValue);  // out with the old
                  *oldValue = valStr;
                  dbChecksum += CalculateKeyValueChecksum(keyStr, valStr);     // in with the new
               }
               else 
               {
                  // create-new key-value pair
                  if (toyDB.Put(keyStr, valStr) != B_NO_ERROR) return B_ERROR;
                  dbChecksum += CalculateKeyValueChecksum(keyStr, valStr);     // in with the new (there is no old)
               }
            }
         }
         return B_NO_ERROR;

         case TOY_DB_COMMAND_REMOVE_STRINGS:
         {
            for (MessageFieldNameIterator fnIter(*cmdMsg(), B_STRING_TYPE); fnIter.HasData(); fnIter++)
            {
               const String & keyStr = fnIter.GetFieldName();
               const String * oldValue = toyDB.Get(keyStr);  // may be NULL if entry requested to delete doesn't exist
               if (oldValue) 
               {
                  dbChecksum -= CalculateKeyValueChecksum(keyStr, *oldValue);  // out with the old (there is no new)
                  (void) toyDB.Remove(keyStr);
               }
            }
         }
         return B_NO_ERROR;

         default:
            LogTime(MUSCLE_LOG_ERROR, "TestZGPeerSession::HandleUpdate:  Unknown command code " UINT32_FORMAT_SPEC "\n", cmdWhat);
         return B_ERROR;
      }
   }

   void SendRandomDatabaseUpdateRequest()
   {
      const int r = GetRandomNumber(&_seed) % 50;

      String s;
      switch(GetRandomNumber(&_seed)%2)
      {
         case 0:  s = String("%1=%1").Arg(r);  break;
         case 1:  s = String("del %1").Arg(r); break;
      }

      printf("AutoTimer: Simulating text command [%s]\n", s());
      TextCommandReceived(s);
   }

   // Returns the incremental checksum value representing a single key-value pair
   uint32 CalculateKeyValueChecksum(const String & keyStr, const String & valStr) const {return ((keyStr.CalculateChecksum()*5) + valStr.CalculateChecksum());}

   Hashtable<String, String> _toyDatabases[NUM_TOY_DATABASES];

   unsigned _seed;
   uint64 _autoUpdateDelay;
   uint64 _nextAutoUpdateTime;
   uint64 _nextPrintNetworkTimeTime;
   uint64 _prevNetworkTime;
   uint64 _prevLocalTime;
   bool _printDBPending;
};

int main(int argc, char ** argv)
{
   int exitCode = 10;

#ifdef UNCOMMENT_THIS_IF_YOU_WANT_TO_TEST_NETWORK_CLOCK_FUNCTIONALITY_ON_A_SINGLE_HOST
   SetPerProcessRunTime64Offset(getpid()*5000000);
#endif

   // This object is required by the MUSCLE library; 
   // it does various system-specific startup and shutdown tasks
   CompleteSetupSystem css;

   // Handling of various optional command-line arguments
   Message args; (void) ParseArgs(argc, argv, args);
   args.AddString("debugcrashes", "");  // let's make sure to print a stack trace if we crash
   HandleStandardDaemonArgs(args);

   // Our test_peer business logic is all implemented inside this object
   TestZGPeerSession zgPeerSession(args);

   // This object will read from stdin for us, so we can accept typed text commands from the user
   ZGStdinSession zgStdinSession(zgPeerSession, true);

   // This object implements the standard MUSCLE event loop and network services
   ReflectServer server;

   // Add our session objects to the ReflectServer object so that they will be used during program execution
   if (((IsDaemonProcess())||(server.AddNewSession(ZGStdinSessionRef(&zgStdinSession, false)) == B_NO_ERROR))&&
       (server.AddNewSession(ZGPeerSessionRef(&zgPeerSession, false)) == B_NO_ERROR))
   {
      // Virtually all of the program's execution time happens inside the ServerProcessLoop() method
      status_t ret = server.ServerProcessLoop();  // doesn't return until it's time to exit
      if (ret == B_NO_ERROR) 
      {
         LogTime(MUSCLE_LOG_INFO, "Event loop exited normally.\n");
         exitCode = 0;
      }
      else LogTime(MUSCLE_LOG_ERROR, "Event loop aborted!\n");

      // Required in order to ensure an orderly shutdown
      server.Cleanup();
   }
   else LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't add TestZGPeerSession!\n");

   return exitCode;
}

