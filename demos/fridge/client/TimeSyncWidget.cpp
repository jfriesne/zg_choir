#include <QMouseEvent>
#include <QPainter>
#include "util/TimeUtilityFunctions.h"
#include "TimeSyncWidget.h"

namespace fridge {

TimeSyncWidget :: TimeSyncWidget(const INetworkTimeProvider * networkTimeProvider)
   : _networkTimeProvider(networkTimeProvider)
   , _animationActive(false)
{
   connect(&_animationTimer, SIGNAL(timeout()), this, SLOT(update()));
}

TimeSyncWidget :: ~TimeSyncWidget()
{
   // empty
}

void TimeSyncWidget :: paintEvent(QPaintEvent *)
{
   QPainter p(this);
   QFontMetrics fm = p.fontMetrics();

   p.fillRect(rect(), Qt::lightGray);

   p.setPen(Qt::black);
   p.drawRect(rect());

   if (_animationActive)
   {
      const uint64 networkNow = _networkTimeProvider ? _networkTimeProvider->GetNetworkTime64() : MUSCLE_TIME_NEVER;
      if (networkNow == MUSCLE_TIME_NEVER) p.drawText(rect(), Qt::AlignCenter, tr("Network time not available"));
      else
      {
         static const uint64 periodMicros = SecondsToMicros(2);
         static const uint64 percentMax = 100*100;  // scale up to avoid too much round-off error
         const uint64 percent = (percentMax*(networkNow%periodMicros))/periodMicros;
         const uint64 bouncePercent = 2*((percent >= (percentMax/2)) ? (percentMax-percent) : (percent));
         const int x = (width()*bouncePercent)/percentMax;
         const int w = muscleMax(width()/20, 6);
         p.fillRect(QRect(x-(w/2), 0, w, height()), Qt::red);

         const uint64 seconds = (networkNow/periodMicros) % 100;
         p.drawText(rect(), Qt::AlignCenter, tr("%1").arg(seconds));
      }
   }
   else p.drawText(rect(), Qt::AlignCenter, tr("Click to animate"));
}

void TimeSyncWidget :: mousePressEvent(QMouseEvent * e)
{
   e->accept(); 
   SetAnimationActive(!IsAnimationActive());
}

void TimeSyncWidget :: SetAnimationActive(bool active)
{
   if (active != _animationActive)
   {
      _animationActive = !_animationActive;
      if (_animationActive) _animationTimer.start(50);
                       else _animationTimer.stop();
      update();
   }
}
      
}; // end namespace fridge
