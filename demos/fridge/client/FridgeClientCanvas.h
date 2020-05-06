#ifndef FridgeClientCanvas_h
#define FridgeClientCanvas_h

#include <QWidget>

#include "zg/messagetree/gateway/ITreeGatewaySubscriber.h"
#include "MagnetState.h"

namespace fridge {

/** Fridge-magnets drawing area. */
class FridgeClientCanvas : public QWidget, public ITreeGatewaySubscriber
{
Q_OBJECT

public:
   /** Constructor */
   FridgeClientCanvas(ITreeGateway * connector);

   /** Destructor */
   virtual ~FridgeClientCanvas();

   virtual void paintEvent(QPaintEvent * e);
   virtual void mousePressEvent(QMouseEvent * e);
   virtual void mouseMoveEvent(QMouseEvent * e);
   virtual void mouseReleaseEvent(QMouseEvent * e);
   
   // ITreeGatewaySubscriber API
   virtual void TreeGatewayConnectionStateChanged();
   virtual void TreeNodeUpdated(const String & nodePath, const MessageRef & optPayloadMsg);

private:
   Hashtable<String, MagnetState> _magnets;
};

}; // end namespace fridge

#endif
