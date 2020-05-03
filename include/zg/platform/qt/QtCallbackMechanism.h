#ifndef QtSocketCallbackMechanism_h
#define QtSocketCallbackMechanism_h

#include "zg/callback/SocketCallbackMechanism.h"

namespace zg {

/** This class implements the ICallbackMechanism interface
  * in a Qt-specific way that makes it easy to integrate
  * ZG callbacks into a Qt-based GUI program.
  */
class QtSocketCallbackMechanism : public SocketCallbackMechanism
{
Q_OBJECT

public:
   /** Constructor 
     * @param optParent passed to the QObject constructor.
     */
   QtSocketCallbackMechanism(QObject * optParent = NULL)
      : QObject(optParent)
      , _notifier(GetDispatchThreadNotifierSocket().GetFileDescriptor(), QSocketNotifier::Read, optParent)
   {
      QObject::connect(&_notifier, SIGNAL(activated(int)), SLOT(NotifierActivated()));
   }

   /** Destructor */
   virtual ~SocketCallbackMechanism()
   {
      _notifier.setEnabled(false);
   }

private slots:
   void NotifierActivated() {DispatchCallbacks();}

   QSocketNotifier _notifier;
};

};  // end zg namespace

#endif
