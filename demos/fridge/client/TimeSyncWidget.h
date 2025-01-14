#ifndef TimeSyncWidget_h
#define TimeSyncWidget_h

#include <QWidget>
#include <QTimer>

#include "common/FridgeNameSpace.h"
#include "zg/INetworkTimeProvider.h"

namespace fridge {

/** This widget implements a silly Knight-Rider style animation based on the current network time.
  * It is here solely to make it easy to see if clients' network-clocks are sync'd together well, or not.
  */
class TimeSyncWidget : public QWidget
{
Q_OBJECT

public:
   /** Constructor
     * @param networkTimeProvider pointer to the object we should call GetNetworkTime64() on to get the current network time.
     */
   TimeSyncWidget(const INetworkTimeProvider * networkTimeProvider);

   /** Destructor */
   virtual ~TimeSyncWidget();

   virtual void paintEvent(QPaintEvent * e);
   virtual void mousePressEvent(QMouseEvent * e);

   void SetAnimationActive(bool a);
   bool IsAnimationActive() const {return _animationActive;}

signals:
   void clicked();

private:
   const INetworkTimeProvider * _networkTimeProvider;

   bool _animationActive;
   QTimer _animationTimer;
};

}; // end namespace fridge

#endif
