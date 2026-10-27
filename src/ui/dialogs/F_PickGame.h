#pragma once

#include <QDialog>
#include <QStringList>

class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

// Lets the user pick among the games that look like a name the exact lookup missed. Knows
// nothing about the scraper: it asks for a search through searchRequested and shows the rows
// it is given through setRows.
class Frm_PickGame : public QDialog
{
   Q_OBJECT

public:
   explicit Frm_PickGame( QWidget* aParent = nullptr );

   void setHint( const QString& aText );
   // Live: searchRequested on every edit. Otherwise on Enter or the Search button, for
   // searches that cost a request.
   void setLiveSearch( bool aLive );
   // Replaces the list and selects the first row.
   void setRows( const QStringList& aRows );

   // The picked row, or -1 if the user cancelled.
   int Execute( const QString& aSearch );

signals:
   void searchRequested( const QString& aText );

protected:
   void keyPressEvent( QKeyEvent* aEvent ) override;

private:
   bool FLive = true;
   QLineEdit* Edt_Search = nullptr;
   QLabel* Lbl_Hint = nullptr;
   QPushButton* Btn_Search = nullptr;
   QListWidget* Lst_Candidates = nullptr;
   QDialogButtonBox* Box_Buttons = nullptr;
};
