#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

class LibraryController;

// The actions on the loaded system that show a review list first. Only Add missing ROMs so far.
class ActionsController : public QObject
{
   Q_OBJECT
   Q_PROPERTY( QStringList roms READ roms NOTIFY romsChanged )
   Q_PROPERTY( int added READ added NOTIFY romsChanged )
   Q_PROPERTY( QString error READ error NOTIFY romsChanged )

public:
   explicit ActionsController( LibraryController* aLibrary, QObject* aParent = nullptr );

   // The candidates of the last findMissingRoms(), as paths relative to the system folder.
   const QStringList& roms() const { return FNames; }
   // How many games the last addRoms() added.
   int added() const { return FAdded; }
   const QString& error() const { return FError; }

   // Fills roms() with the ROMs in the system folder the gamelist does not list. False (with
   // error()) when no system is loaded; an empty result is a success.
   // Limitation: scans on the UI thread. Upgrade: a worker plus a busy flag.
   Q_INVOKABLE bool findMissingRoms();
   // Adds the candidates at aRows (indices into roms()), then reloads the system so the games
   // list follows. False (with error()) when the write failed; the gamelist is then unchanged.
   Q_INVOKABLE bool addRoms( const QVariantList& aRows );

signals:
   void romsChanged();

private:
   void fail( const QString& aError );

   LibraryController* FLibrary;
   QStringList FPaths;
   QStringList FNames;
   int FAdded = 0;
   QString FError;
};
