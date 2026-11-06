#pragma once

#include <QDialog>

class QCheckBox;
class QTextBrowser;

// Short tips at startup, the full help from the Help menu.
class Frm_Help : public QDialog
{
   Q_OBJECT

public:
   explicit Frm_Help( QWidget* aParent = nullptr );

   // With aAllowOptOut, shows the short startup tips with an opt-out checkbox and returns
   // the new "show tips at start" setting. Without, shows the full help and returns aShowTips.
   bool Execute( bool aShowTips, bool aAllowOptOut );

private:
   QTextBrowser* Red_Help = nullptr;
   QCheckBox* Chk_ShowTips = nullptr;
};
