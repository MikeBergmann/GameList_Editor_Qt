#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

class QListWidget;

// Picks the ROMs to add to a gamelist: the files the system folder holds that
// the gamelist does not list, all ticked, plus a Browse button for any other
// file in that folder.
class Frm_AddRoms : public QDialog
{
   Q_OBJECT

public:
   explicit Frm_AddRoms( QWidget* aParent = nullptr );

   // The ticked files as absolute paths. Empty if the user cancelled.
   QStringList Execute( const QString& aSystemDir, const QStringList& aCandidates );

private:
   void browse();
   void addRom( const QString& aPath );

   QString FSystemDir;
   QListWidget* Lst_Roms = nullptr;
};
