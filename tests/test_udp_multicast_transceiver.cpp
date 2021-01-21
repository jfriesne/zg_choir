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

   virtual void MulticastUDPPacketReceived(const IPAddressAndPort & sourceIAP, const ByteBufferRef & packetBytes)
   {
      printf("\n\n");
      LogTime(MUSCLE_LOG_INFO, "Received multicast packet from %s:  [%s]\n", sourceIAP.ToString()(), packetBytes()->GetBuffer());
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

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;     // set up MUSCLE environment
   SocketCallbackMechanism scm; // orchestrates safe calling of callback-methods in the main/user/GUI thread

   const String transmissionKey = (argc > 1) ? argv[1] : "FooBar";

   StdinDataIO stdinDataIO(false);
   PlainTextMessageIOGateway plainTextGateway;  // for parsing data read from stdin
   plainTextGateway.SetDataIO(DataIORef(&stdinDataIO, false));

   LogTime(MUSCLE_LOG_INFO, "This program implements a super-rudimentary text chat via IPv6 UDP multicast packets.\n");
   LogTime(MUSCLE_LOG_INFO, "You can run several instances of it, type text into one instance, and see it appear in the other instances.\n");

   status_t ret;
   UDPMulticastTransceiver multicastTransceiver(&scm);
   if (multicastTransceiver.Start(transmissionKey).IsOK(ret))
   {
      TestMulticastNotificationTarget testTarget(&multicastTransceiver);

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
                        ByteBufferRef payloadBytes = GetByteBufferFromPool(nextLine->FlattenedSize(), (const uint8 *) nextLine->Cstr());
                        if ((payloadBytes())&&(multicastTransceiver.SendMulticastPacket(payloadBytes).IsOK(ret)))
                        {
                           LogTime(MUSCLE_LOG_INFO, "Sent multicast packet containing:  [%s]\n", nextLine->Cstr());
                        }
                        else LogTime(MUSCLE_LOG_ERROR, "Error sending multicast packet containing [%s]: [%s]\n", nextLine->Cstr(), ret());
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
