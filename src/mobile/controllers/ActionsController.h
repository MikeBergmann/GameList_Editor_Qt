#pragma once

#include "Gamelist.h"
#include "ImageOps.h"

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVector>

#include <atomic>

class GamesController;
class LibraryController;
class QThread;

// The actions on the loaded system that show a review list first: find() lists the candidates,
// the user unticks what to skip, apply() writes the rest.
class ActionsController : public QObject
{
   Q_OBJECT
   Q_PROPERTY( QString title READ title NOTIFY itemsChanged )
   Q_PROPERTY( QString intro READ intro NOTIFY itemsChanged )
   Q_PROPERTY( QString applyLabel READ applyLabel NOTIFY itemsChanged )
   Q_PROPERTY( QString emptyText READ emptyText NOTIFY itemsChanged )
   Q_PROPERTY( QVariantList items READ items NOTIFY itemsChanged )
   Q_PROPERTY( QString choiceLabel READ choiceLabel NOTIFY itemsChanged )
   Q_PROPERTY( bool choice READ choice WRITE setChoice NOTIFY itemsChanged )
   Q_PROPERTY( QString report READ report NOTIFY reportChanged )
   Q_PROPERTY( QString error READ error NOTIFY reportChanged )
   Q_PROPERTY( bool busy READ isBusy NOTIFY busyChanged )
   Q_PROPERTY( int progress READ progress NOTIFY busyChanged )
   Q_PROPERTY( int total READ total NOTIFY busyChanged )

public:
   enum Action { AddRoms, LinkMedia, RenameMedia, ShrinkImages, DeleteOrphans, DeleteDuplicates };
   Q_ENUM( Action )

   // aGames, when given, is told to redraw after changes that keep the game list as it is.
   explicit ActionsController( LibraryController* aLibrary, GamesController* aGames = nullptr,
                               QObject* aParent = nullptr );
   ~ActionsController() override;

   const QString& title() const { return FTitle; }
   const QString& intro() const { return FIntro; }
   const QString& applyLabel() const { return FApplyLabel; }
   // What to tell the user when find() succeeded with nothing to review.
   const QString& emptyText() const { return FEmptyText; }
   // One map per candidate: text, enabled. A disabled row is shown unticked and cannot be applied.
   const QVariantList& items() const { return FItems; }
   // A checkbox the review page offers above the list; empty for none.
   const QString& choiceLabel() const { return FChoiceLabel; }
   bool choice() const { return FChoice; }
   // The outcome of the last apply(), and why a find() or apply() failed.
   const QString& report() const { return FReport; }
   const QString& error() const { return FError; }
   bool isBusy() const { return FBusy; }
   int progress() const { return FProgress; }
   int total() const { return FTotal; }

   void setChoice( bool aOn );

   // Fills items(). False (with error()) when no system is loaded or the action has no use here
   // (linking and renaming under ES-DE); an empty list is a success. aMegabytes is the size limit
   // of ShrinkImages and is ignored by the others.
   // Limitation: scans on the UI thread. Upgrade: a worker plus a busy flag.
   Q_INVOKABLE bool find( int aAction, double aMegabytes = 1.0 );
   // Writes the candidates at aRows (indices into items()), then updates the games list.
   // finished() follows, at once except for ShrinkImages, which runs on a worker.
   // Limitation: only the shrink is off the UI thread; renaming many files on a slow card blocks.
   Q_INVOKABLE void apply( const QVariantList& aRows );
   // Stops ShrinkImages before the next picture.
   Q_INVOKABLE void cancel() { FCancel = true; }

signals:
   void itemsChanged();
   void reportChanged();
   void busyChanged();
   void finished( bool aOk );

private:
   void fail( const QString& aError );
   void succeed( const QString& aReport );
   void clearItems();
   void fillItems();
   void applyShrink( const QVector<OversizedImage>& aChosen );
   // The games were added or removed: the list is reloaded so the games model resets.
   bool reloadSystem();
   void redraw();
   QString relative( const QString& aPath ) const;

   LibraryController* FLibrary;
   GamesController* FGames;

   Action FAction = AddRoms;
   QString FTitle, FIntro, FApplyLabel, FEmptyText, FChoiceLabel;
   bool FChoice = false;
   QVariantList FItems;
   QString FReport, FError;

   QStringList FPaths;
   QVector<MediaLink> FLinks;
   QVector<MediaRename> FRenames;
   QVector<OversizedImage> FImages;
   QVector<int> FIndexes;
   qint64 FMaxBytes = 0;

   QThread* FThread = nullptr;
   std::atomic<bool> FCancel{ false };
   bool FBusy = false;
   int FProgress = 0;
   int FTotal = 0;
};
