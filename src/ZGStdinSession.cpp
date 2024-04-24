#include "dataio/StdinDataIO.h"
#include "iogateway/PlainTextMessageIOGateway.h"
#include "system/SystemInfo.h"  // for PrintBuildFlags()
#include "util/NetworkUtilityFunctions.h"
#include "util/StringTokenizer.h"
#include "zg/ZGStdinSession.h"

namespace zg {

ZGStdinSession :: ZGStdinSession(ITextCommandReceiver & target, bool endServerOnClose) : _target(target), _endServerOnClose(endServerOnClose), _calledEndServer(false)
{
   /* empty */
}

DataIORef ZGStdinSession :: CreateDataIO(const ConstSocketRef &)
{
   return DataIORef(new StdinDataIO(false));
}

bool ZGStdinSession :: IsReallyStdin() const
{
   const AbstractMessageIOGateway * gw = GetGateway()();
   return ((gw)&&(dynamic_cast<StdinDataIO *>(gw->GetDataIO()()) != NULL));
}

bool ZGStdinSession :: ClientConnectionClosed()
{
   bool isReallyStdin = IsReallyStdin();
   if (_endServerOnClose)
   {
      if (isReallyStdin) LogTime(MUSCLE_LOG_DEBUG, "ZGStdinSession:  stdin was closed -- this process will end shortly!\n");
      EndServer();  // we want our process to go away if we lose the stdin/stdout connection to the parent process
      _calledEndServer = true;
   }
   else if (isReallyStdin) LogTime(MUSCLE_LOG_DEBUG, "ZGStdinSession:  stdin was closed, but this process will continue running anyway.\n");

   return AbstractReflectSession::ClientConnectionClosed();
}

bool ZGStdinSession :: IsReadyForInput() const
{
   return ((AbstractReflectSession::IsReadyForInput())&&(_target.IsReadyForTextCommands()));
}

AbstractMessageIOGatewayRef ZGStdinSession :: CreateGateway()
{
   return AbstractMessageIOGatewayRef(new PlainTextMessageIOGateway);
}

void ZGStdinSession :: MessageReceivedFromGateway(const MessageRef & msg, void * /*ptr*/)
{
   if ((msg())&&(msg()->what == PR_COMMAND_TEXT_STRINGS))
   {
      String nextCmd;
      for (int32 i=0; msg()->FindString(PR_NAME_TEXT_LINE, i, nextCmd).IsOK(); i++)
      {
         nextCmd = nextCmd.Trimmed();
         StringTokenizer tok(nextCmd(), ";;");
         const char * t;
         while((t = tok()) != NULL)
         {
            String nc = t; nc = nc.Trimmed();  // yes, the Trimmed() is necessary!
            if ((nc == "quit")||(nc == "exit"))
            {
               if (IsReallyStdin()) printf("To close a stdin session, press Control-D.\n");
               else
               {
                  printf("Closing command session...\n");
                  EndSession();
               }
            }
            else if ((_target.TextCommandReceived(nc) == false)&&(nc.HasChars())) printf("ZGStdinSession:  Could not parse stdin command string [%s] (cmdLen=" UINT32_FORMAT_SPEC ")\n", nc(), nc.Length());
         }
      }
   }
}

static void LogAux(const String & s, int sev)
{
   String logText = s.Substring(s.IndexOf(' ')+1);
   if ((logText.Length() >= 2)&&(logText.StartsWith("/")))
   {
      for (uint32 i=0; i<NUM_MUSCLE_LOGLEVELS; i++)
      {
         if (logText[1] == GetLogLevelKeyword(i)[0])
         {
            sev = i;
            logText = logText.Substring(2);
            break;
         }
      }
   }
   LogTime(sev, "%s\n", logText());
}

static bool HandleSetLogLevelCommand(const char * p, bool isDisplay)
{
   while((*p)&&(muscleInRange(*p, 'A', 'Z') == false)&&(muscleInRange(*p, 'a', 'z') == false)) p++;

   uint32 logLevel = NUM_MUSCLE_LOGLEVELS;
   String lvl = p;  lvl = lvl.Trimmed();
   if (lvl.HasChars())
   {
      for (uint32 i=0; i<NUM_MUSCLE_LOGLEVELS; i++)
      {
         if (String(GetLogLevelKeyword(i)).StartsWithIgnoreCase(lvl))
         {
            logLevel = i;
            break;
         }
      }
   }

   if (logLevel == NUM_MUSCLE_LOGLEVELS)
   {
      LogTime(MUSCLE_LOG_ERROR, "Could not parse %s level keyword [%s]\n", isDisplay?"display":"log", lvl());
      return false;
   }
   else
   {
      if (isDisplay) (void) SetConsoleLogLevel(logLevel);
                else (void) SetFileLogLevel(logLevel);
      return true;
   }
}

bool ITextCommandReceiver :: ParseGenericTextCommand(const String & s)
{
        if (s.StartsWith("echo ")) printf("echoing: [%s]\n", s.Substring(5).Trimmed()());
   else if (s.StartsWith("crit"))  LogAux(s, MUSCLE_LOG_CRITICALERROR);
   else if (s.StartsWith("err"))   LogAux(s, MUSCLE_LOG_ERROR);
   else if (s.StartsWith("warn"))  LogAux(s, MUSCLE_LOG_WARNING);
   else if (s.StartsWith("log"))   LogAux(s, MUSCLE_LOG_INFO);
   else if (s.StartsWith("debug")) LogAux(s, MUSCLE_LOG_DEBUG);
   else if (s.StartsWith("trace")) LogAux(s, MUSCLE_LOG_TRACE);
   else if (s.StartsWith("sleep"))
   {
      const uint64 micros = ParseHumanReadableTimeIntervalString(s.Substring(5).Trimmed());
      const char * preposition = (micros==MUSCLE_TIME_NEVER)?"":"for ";
      LogTime(MUSCLE_LOG_INFO, "Sleeping %s%s...\n", preposition, GetHumanReadableTimeIntervalString(micros)());
      (void) Snooze64(micros);
      LogTime(MUSCLE_LOG_INFO, "Awoke after sleeping %s%s\n", preposition, GetHumanReadableTimeIntervalString(micros)());
   }
   else if (s.StartsWith("spin"))
   {
      const uint64 micros = ParseHumanReadableTimeIntervalString(s.Substring(5).Trimmed());
      LogTime(MUSCLE_LOG_INFO, "Spinning for %s...\n", GetHumanReadableTimeIntervalString(micros)());
      const uint64 endTime = (micros == MUSCLE_TIME_NEVER) ? MUSCLE_TIME_NEVER : (GetRunTime64()+micros);
      while(GetRunTime64()<endTime) {/* spin, my little process, spin! */}
      LogTime(MUSCLE_LOG_INFO, "Finished spinning for %s\n", GetHumanReadableTimeIntervalString(micros)());
   }
   else if (s == "print object counts") PrintCountedObjectInfo();
   else if (s == "print all network interfaces")
   {
      printf("List of all network interfaces known to this host:\n");
      Queue<NetworkInterfaceInfo> niis; (void) GetNetworkInterfaceInfos(niis);
      for (uint32 i=0; i<niis.GetNumItems(); i++)
      {
         const NetworkInterfaceInfo & nii = niis[i];
         printf(UINT32_FORMAT_SPEC ". %s\n", i, nii.ToString()());
      }
   }
   else if (s.StartsWith("set displaylevel")) return HandleSetLogLevelCommand(s()+16, true);
   else if (s.StartsWith("set loglevel"))     return HandleSetLogLevelCommand(s()+12, false);
   else if (s == "sdt")                       return HandleSetLogLevelCommand("trace", true);
   else if (s == "sdd")                       return HandleSetLogLevelCommand("debug", true);
   else if (s == "sdi")                       return HandleSetLogLevelCommand("info", true);
   else if (s == "sdw")                       return HandleSetLogLevelCommand("warn", true);
   else if (s == "sde")                       return HandleSetLogLevelCommand("error", true);
   else if (s == "sdc")                       return HandleSetLogLevelCommand("critical", true);
   else if (s == "crash")
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Forcing this process to crash, ka-BOOM!\n");
      Crash();
   }
   else if (s == "print build flags")
   {
      printf("This executable was compiled using MUSCLE version %s.\n", MUSCLE_VERSION_STRING);
      PrintBuildFlags();
   }
   else if (s == "print stack trace") (void) PrintStackTrace();
   else return false;

   return true;
}

};  // end namespace zg
