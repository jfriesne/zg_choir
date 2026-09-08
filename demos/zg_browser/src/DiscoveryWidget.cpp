#include "DiscoveryWidget.h"

#include <algorithm>

#include <QCollator>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include "Theme.h"

#include "zg/discovery/common/DiscoveryUtilityFunctions.h"

using namespace muscle;

namespace {

enum {ROLE_NAME = Qt::UserRole, ROLE_DETAIL};

/** Draws a system as two lines:  its name, and a dim summary line under it. */
class SystemRowDelegate final : public QStyledItemDelegate
{
public:
   explicit SystemRowDelegate(QObject * parent) : QStyledItemDelegate(parent) {/* empty */}

   QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
   {
      return QSize(0, 46);
   }

   void paint(QPainter * p, const QStyleOptionViewItem & opt, const QModelIndex & idx) const override
   {
      const bool selected = opt.state & QStyle::State_Selected;
      if (selected) p->fillRect(opt.rect, zgb::theme::accent);

      const QRect r = opt.rect.adjusted(12, 0, -12, 0);

      QFont nameFont = opt.font;
      nameFont.setBold(true);
      nameFont.setPointSizeF(nameFont.pointSizeF() + 1.0);
      p->setFont(nameFont);
      p->setPen(zgb::theme::text);
      p->drawText(QRect(r.left(), r.top()+5, r.width(), 20), Qt::AlignLeft | Qt::AlignVCenter,
                  QFontMetrics(nameFont).elidedText(idx.data(ROLE_NAME).toString(), Qt::ElideRight, r.width()));

      QFont detailFont = opt.font;
      detailFont.setPointSizeF(detailFont.pointSizeF() - 1.0);
      p->setFont(detailFont);
      p->setPen(selected ? QColor(255, 255, 255, 204) : zgb::theme::textDim);
      p->drawText(QRect(r.left(), r.top()+24, r.width(), 17), Qt::AlignLeft | Qt::AlignVCenter,
                  QFontMetrics(detailFont).elidedText(idx.data(ROLE_DETAIL).toString(), Qt::ElideRight, r.width()));

      p->setPen(zgb::theme::header);
      p->drawLine(opt.rect.left(), opt.rect.bottom(), opt.rect.right(), opt.rect.bottom());
   }
};

/** Natural ("srv2" before "srv10") comparison. */
int compareNatural(const QString & a, const QString & b)
{
   static const QCollator collator = []
   {
      QCollator c;
      c.setNumericMode(true);
      c.setCaseSensitivity(Qt::CaseInsensitive);
      return c;
   }();
   return collator.compare(a, b);
}

}  // anonymous namespace

DiscoveryWidget :: DiscoveryWidget(zg::SystemDiscoveryClient & discoveryClient, QWidget * parent)
   : QWidget(parent)
   , zg::IDiscoveryNotificationTarget(&discoveryClient)
{
   QVBoxLayout * layout = new QVBoxLayout(this);
   layout->setContentsMargins(16, 16, 16, 16);
   layout->setSpacing(0);

   QLabel * title = new QLabel(tr("ZG systems on this network"));
   {
      QFont f = title->font();
      f.setBold(true);
      f.setPointSizeF(f.pointSizeF() + 6.0);
      title->setFont(f);
   }
   layout->addWidget(title);

   QLabel * hint = new QLabel(tr("Click a system to browse its database."));
   hint->setObjectName("hint");
   layout->addWidget(hint);
   layout->addSpacing(8);

   _listBox = new QListWidget;
   _listBox->setItemDelegate(new SystemRowDelegate(_listBox));
   _listBox->setSelectionMode(QAbstractItemView::SingleSelection);
   _listBox->setUniformItemSizes(true);
   connect(_listBox, &QListWidget::currentRowChanged, this, [](int) {/* selection only; activation chooses */});
   connect(_listBox, &QListWidget::itemClicked,   this, [this](QListWidgetItem * it) {chooseRow(_listBox->row(it));});
   connect(_listBox, &QListWidget::itemActivated, this, [this](QListWidgetItem * it) {chooseRow(_listBox->row(it));});
   layout->addWidget(_listBox, 1);

   // Shown over the (empty) list until the first system turns up.
   _emptyLabel = new QLabel(tr("Listening for ZG systems..."), _listBox);
   _emptyLabel->setObjectName("dim");
   _emptyLabel->setAlignment(Qt::AlignCenter);
   _emptyLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
   _listBox->installEventFilter(this);

   layout->addSpacing(10);

   QHBoxLayout * manualRow = new QHBoxLayout;
   manualRow->setContentsMargins(0, 0, 0, 0);
   manualRow->setSpacing(8);
   {
      QLabel * manualLabel = new QLabel(tr("Or connect to a system by name:"));
      manualLabel->setObjectName("dim");
      manualRow->addWidget(manualLabel);

      _manualName = new QLineEdit;
      _manualName->setPlaceholderText(tr("system name (wildcards allowed, eg *)"));
      connect(_manualName, &QLineEdit::returnPressed, this, &DiscoveryWidget::connectToTypedSystemName);
      manualRow->addWidget(_manualName, 1);

      _manualConnect = new QPushButton(tr("Connect"));
      _manualConnect->setObjectName("primary");
      connect(_manualConnect, &QPushButton::clicked, this, &DiscoveryWidget::connectToTypedSystemName);
      manualRow->addWidget(_manualConnect);
   }
   layout->addLayout(manualRow);

   rebuildRows();
}

DiscoveryWidget :: ~DiscoveryWidget()
{
   SetDiscoveryClient(NULL);   // no more callbacks, please
}

bool DiscoveryWidget :: eventFilter(QObject * watched, QEvent * event)
{
   if ((watched == _listBox)&&(event->type() == QEvent::Resize)) _emptyLabel->setGeometry(_listBox->rect());
   return QWidget::eventFilter(watched, event);
}

void DiscoveryWidget :: DiscoveryUpdate(const String & systemName, const MessageRef & optSystemInfo)
{
   if (optSystemInfo()) (void) _systems.Put(systemName, optSystemInfo);
                   else (void) _systems.Remove(systemName);
   rebuildRows();
}

void DiscoveryWidget :: rebuildRows()
{
   // Remember the selection by name, so it survives the list being reordered.
   const int previousRow = _listBox->currentRow();
   const String previousName = ((previousRow >= 0)&&(previousRow < (int) _rows.size()))
                             ? _rows[(size_t) previousRow]._systemName : GetEmptyString();

   _rows.clear();
   _rows.reserve(_systems.GetNumItems());

   for (ConstHashtableIterator<String, MessageRef> iter(_systems); iter.HasData(); iter++)
   {
      SystemRow row;
      row._systemName = iter.GetKey();

      QStringList addresses;
      uint32 numPeers = 0;

      ConstMessageRef peerInfo;
      for (uint32 i=0; iter.GetValue()()->FindMessage(ZG_DISCOVERY_NAME_PEERINFO, i, peerInfo).IsOK(); i++)
      {
         numPeers++;
         if (row._signature.IsEmpty()) row._signature = peerInfo()->GetString(ZG_DISCOVERY_NAME_SIGNATURE);

         const String source = peerInfo()->GetString(ZG_DISCOVERY_NAME_SOURCE);
         if (source.HasChars())
         {
            const QString address = zgb::toQt(source);
            if (addresses.contains(address) == false) addresses << address;
         }
      }

      row._detail = zgb::toQt(row._signature.IsEmpty() ? String("(unknown signature)") : row._signature)
                  + QString("  |  ") + QString::number(numPeers) + (numPeers == 1 ? tr(" peer") : tr(" peers"))
                  + (addresses.isEmpty() ? QString() : ("  |  " + addresses.join(", ")));

      _rows.push_back(row);
   }

   std::sort(_rows.begin(), _rows.end(), [](const SystemRow & a, const SystemRow & b)
   {
      return compareNatural(zgb::toQt(a._systemName), zgb::toQt(b._systemName)) < 0;
   });

   _listBox->clear();
   for (const SystemRow & row : _rows)
   {
      QListWidgetItem * item = new QListWidgetItem(_listBox);
      item->setData(ROLE_NAME,   zgb::toQt(row._systemName));
      item->setData(ROLE_DETAIL, row._detail);
   }

   if (previousName.HasChars())
   {
      for (size_t i=0; i<_rows.size(); i++) if (_rows[i]._systemName == previousName)
      {
         _listBox->setCurrentRow((int) i);
         break;
      }
   }

   _emptyLabel->setVisible(_rows.empty());
   _emptyLabel->setGeometry(_listBox->rect());
}

void DiscoveryWidget :: chooseRow(int rowNumber)
{
   if ((rowNumber < 0)||(rowNumber >= (int) _rows.size())) return;

   const SystemRow & row = _rows[(size_t) rowNumber];
   emit systemChosen(row._signature.IsEmpty() ? String("*") : row._signature, row._systemName);
}

void DiscoveryWidget :: connectToTypedSystemName()
{
   const QString typed = _manualName->text().trimmed();
   if (typed.isEmpty() == false) emit systemChosen(String("*"), zgb::toMuscle(typed));
}
