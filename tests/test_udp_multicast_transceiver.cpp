#include "dataio/StdinDataIO.h"
#include "iogateway/PlainTextMessageIOGateway.h"
#include "reflector/ReflectServer.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"

#include "zg/callback/SocketCallbackMechanism.h"
#include "zg/udp/IUDPMulticastNotificationTarget.h"
#include "zg/udp/UDPMulticastTransceiver.h"
#include "zg/ZGStdinSession.h"

using namespace zg;

class TestMulticastNotificationTarget : public IUDPMulticastNotificationTarget
{
public:
   TestMulticastNotificationTarget(UDPMulticastTransceiver * client) : IUDPMulticastNotificationTarget(client)
   {
      // empty
   }

   virtual void UDPPacketReceived(const IPAddressAndPort & sourceIAP, const ByteBufferRef & packetBytes)
   {
      printf("\n\n");
      LogTime(MUSCLE_LOG_INFO, "Received UDP packet from %s:  [%s]\n", sourceIAP.ToString()(), packetBytes()->GetBuffer());
      PrintHexBytes(packetBytes);
   }

   virtual void ComputerIsAboutToSleep()
   {
      printf("\n\n");
      LogTime(MUSCLE_LOG_INFO, "This computer is about to go to sleep!\n");
   }

   virtual void ComputerJustWokeUp()
   {
      printf("\n\n");
      LogTime(MUSCLE_LOG_INFO, "This computer just woke up.\n");
   }
};

// Looks for a string like "[::1]:1234 foo bar", and if it finds it, sets (outStr) to "foo bar" and returns the IPAddressAndPort.
// otherwise, sets (outStr) equal to (inStr) and returns an invalid IPAddressAndPort.
static IPAddressAndPort ParseUnicastAddressFromBeginningOfString(const String & inStr, String & outStr)
{
   int32 rBracket = inStr.StartsWith('[') ? inStr.IndexOf(']') : -1;
   if (rBracket > 0)
   {
      const IPAddressAndPort ret(inStr, 0, true);
      while((rBracket < inStr.Length())&&(inStr[rBracket] != ' ')) rBracket++;
      outStr = inStr.Substring(rBracket).Trim();
      return ret; 
   }

   // No unicast address parsed
   outStr = inStr;
   return IPAddressAndPort();
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;     // set up MUSCLE environment
   SocketCallbackMechanism scm; // orchestrates safe calling of callback-methods in the main/user/GUI thread

   Message args;
   (void) ParseArgs(argc, argv, args);
   (void) HandleStandardDaemonArgs(args);

   StdinDataIO stdinDataIO(false);
   PlainTextMessageIOGateway plainTextGateway;  // for parsing data read from stdin
   plainTextGateway.SetDataIO(DummyDataIORef(stdinDataIO));

   LogTime(MUSCLE_LOG_INFO, "This program implements a super-rudimentary text chat via IPv6 UDP multicast packets.\n");
   LogTime(MUSCLE_LOG_INFO, "You can run several instances of it, type text into one instance, and see it appear in the other instances.\n");

   const String transmissionKey = args.GetString("key", "ExampleKey");
   const String nicNameFilter   = args.GetString("nics");

   status_t ret;
   UDPMulticastTransceiver multicastTransceiver(&scm);
   if (nicNameFilter.HasChars()) multicastTransceiver.SetNetworkInterfaceNameFilter(nicNameFilter);
   if (multicastTransceiver.Start(transmissionKey).IsOK(ret))
   {
      TestMulticastNotificationTarget testTarget(&multicastTransceiver);

      if (nicNameFilter.HasChars()) LogTime(MUSCLE_LOG_INFO, "Using only network interfaces whose names match the pattern [%s]\n", nicNameFilter());
      LogTime(MUSCLE_LOG_INFO, "Listening for multicast packets from any UDPMulticastTransceivers using transmission-key [%s]\n", transmissionKey());

      SocketMultiplexer sm;
      while(true)
      {
         const int notifySocket = scm.GetDispatchThreadNotifierSocket().GetFileDescriptor();
         const int stdinSocket  = stdinDataIO.GetReadSelectSocket().GetFileDescriptor();

         (void) sm.RegisterSocketForReadReady(notifySocket);
         (void) sm.RegisterSocketForReadReady(stdinSocket);

         if (sm.WaitForEvents() >= 0)
         {
            // Respond to notifications about incoming UDP packets
            if (sm.IsSocketReadyForRead(notifySocket)) scm.DispatchCallbacks();

            // Read text lines from stdin and send them as multicast packets
            if (sm.IsSocketReadyForRead(stdinSocket))
            {
               QueueGatewayMessageReceiver incomingText;
               while(plainTextGateway.DoInput(incomingText) > 0) {/* empty */} // just calling fgets() would be simpler, but it wouldn't work correctly under Windows
               const Queue<MessageRef> & mq = incomingText.GetMessages();
               for (uint32 i=0; i<mq.GetNumItems(); i++)
               {
                  const Message & m = *mq[i]();
                  const String * nextLine;
                  for (uint32 j=0; m.FindString(PR_NAME_TEXT_LINE, j, &nextLine).IsOK(); j++)
                  {
                     if (nextLine->HasChars())
                     {
                        String sendStr = *nextLine;

                        const IPAddressAndPort unicastDestinationAddress = ParseUnicastAddressFromBeginningOfString(nextLine->Trim(), sendStr);

                        ByteBufferRef payloadBytes = GetByteBufferFromPool(sendStr.FlattenedSize(), (const uint8 *) sendStr());
                        if (payloadBytes())
                        {
                           if (unicastDestinationAddress.IsValid())
                           {
                              if (multicastTransceiver.SendUnicastPacket(unicastDestinationAddress, payloadBytes).IsOK(ret))
                              {
                                 LogTime(MUSCLE_LOG_INFO, "Sent unicast packet containing:  [%s] to [%s]\n", sendStr(), unicastDestinationAddress.ToString()());
                              }
                              else LogTime(MUSCLE_LOG_ERROR, "Error sending unicast packet containing [%s] to [%s]: [%s]\n", sendStr(), unicastDestinationAddress.ToString()(), ret());
                           }
                           else if (multicastTransceiver.SendMulticastPacket(payloadBytes).IsOK(ret))
                           {
                              LogTime(MUSCLE_LOG_INFO, "Sent multicast packet containing:  [%s]\n", sendStr());
                           }
                           else LogTime(MUSCLE_LOG_ERROR, "Error sending multicast packet containing [%s]: [%s]\n", sendStr(), ret());
                        }
                     }
                  }
               }
            }
         }
         else
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "WaitForEvents() failed, exiting! [%s]\n", B_ERRNO());
            break;
         }
      }
   }
   else LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't start UDPMulticastTransceiver, exiting! [%s]\n", ret());

   return 0;
}
