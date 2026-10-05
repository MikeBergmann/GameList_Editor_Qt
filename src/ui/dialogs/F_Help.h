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

   // Shows the window and returns the new "show tips at start" setting.
   //
   // aAllowOptOut is what the Delphi expressed by hiding Chk_ShowTips from the
   // menu caller, and getting wrong: that caller passed FShowTips straight in,
   // so Execute returned its negation and every visit from the menu silently
   // flipped the setting. With the opt-out hidden the answer can only be the
   // value that came in, so this says so rather than computing it.
   bool Execute( bool aShowTips, bool aAllowOptOut );

private:
   QTextBrowser* Red_Help = nullptr;
   QCheckBox* Chk_ShowTips = nullptr;
};
