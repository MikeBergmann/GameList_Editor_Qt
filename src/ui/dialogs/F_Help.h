#pragma once

#include <QDialog>

class QCheckBox;
class QTextBrowser;

// The tips window, shown at startup and from the Help menu.
class Frm_Help : public QDialog
{
   Q_OBJECT

public:
   explicit Frm_Help( QWidget* aParent = nullptr );

   // Shows the window and returns the new "show tips at start" setting. With aAllowOptOut
   // false the opt-out checkbox is hidden and aShowTips comes back unchanged.
   bool Execute( bool aShowTips, bool aAllowOptOut );

private:
   QTextBrowser* Red_Help = nullptr;
   QCheckBox* Chk_ShowTips = nullptr;
};
