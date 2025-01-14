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
   /** Constructor
     * @param connector the ITreeGateway we should register with and use for our database access
     */
   FridgeClientCanvas(ITreeGateway * connector);

   /** Destructor */
   virtual ~FridgeClientCanvas();

   virtual void paintEvent(QPaintEvent * e);
   virtual void mousePressEvent(QMouseEvent * e);
   virtual void mouseMoveEvent(QMouseEvent * e);
   virtual void mouseReleaseEvent(QMouseEvent * e);

   // ITreeGatewaySubscriber API
   virtual void TreeGatewayConnectionStateChanged();
   virtual void TreeNodeUpdated(const String & nodePath, const ConstMessageRef & optPayloadMsg, const String & optOpTag);
   virtual void MessageReceivedFromTreeSeniorPeer(int32 whichDB, const String & tag, const MessageRef & payload);

   /** Uploads a request to the server to clear all the magnets from the fridge */
   void ClearMagnets();

   /** Returns true iff there are any magnets present on the refrigerator door */
   bool HasMagnets() const {return _magnets.HasItems();}

signals:
   void UpdateWindowStatus();

private:
   String GetMagnetAtPoint(const QPoint & pt) const;
   status_t UploadMagnetState(const String & optNodeID, const MagnetState * optMagnetState, bool isInterimUpdate);
   void UpdateDraggedMagnetPosition(QPoint mousePos, bool isInterimUpdate);
   void UploadNewMagnet(int x, int y, const String & word);

   Hashtable<String, MagnetState> _magnets;
   String _draggingID;  // if non-empty, we're in the middle of moving a magnet
   QPoint _dragDelta;   // mouse-click position minus upper-left position

   MagnetState _lastDraggedMagnet;  // mainly here to test out symlink functionality

   bool _firstMouseMove;
};

}; // end namespace fridge

#endif
