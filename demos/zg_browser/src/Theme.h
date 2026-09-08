#ifndef Theme_h
#define Theme_h

#include <QColor>
#include <QString>

/** The app's colour palette, in one place so the widgets and the stylesheet
  * can't drift apart.
  */
namespace zg_browser::theme
{

inline const QColor background        {0x1e, 0x1e, 0x22};   ///< window / tree background
inline const QColor contentBackground {0x17, 0x17, 0x1a};   ///< message pane, lists, text fields
inline const QColor header            {0x2a, 0x2a, 0x30};   ///< the top bar
inline const QColor border            {0x3a, 0x3a, 0x42};   ///< hairlines and control outlines

inline const QColor control           {0x33, 0x33, 0x3c};   ///< button face
inline const QColor accent            {0x2f, 0x5d, 0x8f};   ///< selection, primary button
inline const QColor overlay           {0x10, 0x10, 0x13, 0xd0};  ///< "disconnected" cover

inline const QColor text              {0xf0, 0xf0, 0xf4};   ///< primary text
inline const QColor textBody          {0xd8, 0xd8, 0xdc};   ///< monospaced dumps, editable text
inline const QColor textDim           {0x8e, 0x8e, 0x98};   ///< secondary/annotation text

inline const QColor connected         {0x7b, 0xd8, 0x8f};
inline const QColor disconnected      {0xe0, 0xa0, 0x4a};

/** The whole app's styling, applied once to the QApplication so that every
  * widget picks up the palette above without restyling itself.
  */
inline QString appStyleSheet()
{
   const QString bg      = background.name();
   const QString content = contentBackground.name();
   const QString hdr     = header.name();
   const QString brd     = border.name();
   const QString ctl     = control.name();
   const QString acc     = accent.name();
   const QString txt     = text.name();
   const QString body    = textBody.name();
   const QString dim     = textDim.name();

   return QString(
      "QWidget { background: %1; color: %7; }"
      "QLabel { background: transparent; }"
      "QLabel#dim, QLabel#hint { color: %9; }"

      "QPushButton {"
      "   background: %5; color: %7; border: 1px solid %4;"
      "   border-radius: 5px; padding: 5px 12px; }"
      "QPushButton:hover    { background: %5; border-color: %6; }"
      "QPushButton:pressed  { background: %4; }"
      "QPushButton:disabled { color: %9; }"
      "QPushButton#primary  { background: %6; border-color: %6; color: white; }"

      "QLineEdit {"
      "   background: %2; color: %8; border: 1px solid %4;"
      "   border-radius: 5px; padding: 4px 8px;"
      "   selection-background-color: %6; selection-color: white; }"
      "QLineEdit:focus { border-color: %6; }"

      "QTextEdit, QPlainTextEdit {"
      "   background: %2; color: %8; border: none;"
      "   selection-background-color: %6; selection-color: white; }"

      "QTreeWidget, QTreeView {"
      "   background: %1; color: %7; border: none;"
      "   alternate-background-color: %1; outline: none; }"
      "QTreeView::item { padding: 2px 0px; }"
      "QTreeView::item:selected { background: %6; color: white; }"
      "QHeaderView::section {"
      "   background: %3; color: %9; border: none;"
      "   border-bottom: 1px solid %4; padding: 4px 6px; }"

      "QListWidget {"
      "   background: %2; color: %7; border: none; outline: none; }"
      "QListWidget::item:selected { background: %6; color: white; }"

      "QSplitter::handle { background: %4; }"

      "QScrollBar:vertical   { background: %2; width: 11px; margin: 0; }"
      "QScrollBar:horizontal { background: %2; height: 11px; margin: 0; }"
      "QScrollBar::handle { background: %4; border-radius: 5px; min-height: 24px; min-width: 24px; }"
      "QScrollBar::handle:hover { background: %6; }"
      "QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }"
      "QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }"
   ).arg(bg, content, hdr, brd, ctl, acc, txt, body, dim);
}

}  // namespace zg_browser::theme

#endif // Theme_h
