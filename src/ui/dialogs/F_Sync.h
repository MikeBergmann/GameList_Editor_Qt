#pragma once

#include "Sync.h"

#include <QDialog>

class QPlainTextEdit;
class QPushButton;
class QTreeWidget;

// What one system's plan would do, game by game, as plain text for the details window.
QString syncDetails( const SystemPlan& aSystem );

// Reviews what planSync found: one row per system with its counts and a Details button,
// ticked when there is something to do, and the games left alone and the problems below.
class Frm_Sync : public QDialog
{
   Q_OBJECT

public:
   explicit Frm_Sync( QWidget* aParent = nullptr );

   // The plan cut down to the ticked systems. Empty if the user cancelled.
   SyncPlan Execute( const SyncPlan& aPlan );

private:
   void updateStart();
   void showDetails( const SystemPlan& aSystem );

   QTreeWidget* Lst_Systems = nullptr;
   QPlainTextEdit* Txt_Notes = nullptr;
   QPushButton* Btn_Start = nullptr;
};
