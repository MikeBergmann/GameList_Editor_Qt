#pragma once

#include <QDialog>

class QLineEdit;

struct Game;

// Read-only detail view: play count, last played, and the three hashes.
class Frm_MoreInfos : public QDialog
{
   Q_OBJECT

public:
   explicit Frm_MoreInfos( QWidget* aParent = nullptr );

   void Execute( const Game& aGame );

private:
   QLineEdit* Edt_Playcount = nullptr;
   QLineEdit* Edt_LastPlayed = nullptr;
   QLineEdit* Edt_Crc32 = nullptr;
   QLineEdit* Edt_Md5 = nullptr;
   QLineEdit* Edt_Sha1 = nullptr;
};
