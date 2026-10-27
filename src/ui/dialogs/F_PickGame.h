#pragma once

#include "LaunchBoxDb.h"

#include <QDialog>

class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QListWidget;

// Lets the user pick among the LaunchBox games that look like a name the exact lookup missed.
class Frm_PickGame : public QDialog
{
   Q_OBJECT

public:
   // aDb must outlive the dialog.
   explicit Frm_PickGame( const LaunchBoxDb& aDb, QWidget* aParent = nullptr );

   // The picked game's index for LaunchBoxDb::infoAt, or -1 if the user cancelled.
   int Execute( const QString& aSearch );

private:
   void search();

   const LaunchBoxDb& FDb;
   QLineEdit* Edt_Search = nullptr;
   QLabel* Lbl_Hint = nullptr;
   QListWidget* Lst_Candidates = nullptr;
   QDialogButtonBox* Box_Buttons = nullptr;
};
