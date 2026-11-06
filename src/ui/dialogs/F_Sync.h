#pragma once

#include "Sync.h"

#include <QDialog>

class QListWidget;
class QPlainTextEdit;
class QPushButton;

// Reviews what planSync found: one row per system with its counts, ticked when there is
// something to do, and the games left alone and the problems below.
class Frm_Sync : public QDialog
{
   Q_OBJECT

public:
   explicit Frm_Sync( QWidget* aParent = nullptr );

   // The plan cut down to the ticked systems. Empty if the user cancelled.
   SyncPlan Execute( const SyncPlan& aPlan );

private:
   void updateStart();

   QListWidget* Lst_Systems = nullptr;
   QPlainTextEdit* Txt_Notes = nullptr;
   QPushButton* Btn_Start = nullptr;
};
