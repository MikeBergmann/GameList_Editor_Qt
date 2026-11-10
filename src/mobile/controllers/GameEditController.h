#pragma once

#include "Game.h"
#include "Result.h"

#include <QObject>
#include <QString>
#include <QVariantMap>

class GamesController;
class LibraryController;

// The game selected in GamesController, as the edit page sees it: the 12 fields (typed values are
// kept here, not in the Gamelist, until save), the dirty flag and the picture and video actions.
class GameEditController : public QObject
{
   Q_OBJECT
   Q_PROPERTY( QVariantMap values READ values NOTIFY valuesChanged )
   Q_PROPERTY( bool dirty READ isDirty NOTIFY dirtyChanged )
   Q_PROPERTY( QString error READ error NOTIFY errorChanged )
   Q_PROPERTY( QString imageSource READ imageSource NOTIFY mediaChanged )
   Q_PROPERTY( bool hasVideo READ hasVideo NOTIFY mediaChanged )

public:
   GameEditController( LibraryController* aLibrary, GamesController* aGames, QObject* aParent = nullptr );

   // The 12 editable fields by key (name, genre, rating, players, developer, publisher,
   // releaseDate, description, region, kidGame, hidden, favorite) plus the read-only romPath.
   QVariantMap values() const;
   bool isDirty() const;
   const QString& error() const { return FError; }
   // A file URL, empty without a picture.
   QString imageSource() const;
   bool hasVideo() const;

   // Unknown keys are ignored.
   Q_INVOKABLE void setField( const QString& aKey, const QVariant& aValue );
   // False (with error()) when the write failed; the typed values stay.
   Q_INVOKABLE bool save();
   // Drops the typed values.
   Q_INVOKABLE void revert();
   // Typed values survive both. False (with error()) when the write failed.
   Q_INVOKABLE bool removeImage();
   Q_INVOKABLE bool removeVideo();

signals:
   void valuesChanged();
   void dirtyChanged();
   void errorChanged();
   void mediaChanged();

private:
   void load( int aIndex );
   bool finishMedia( const Result<>& aResult );
   const Game* game() const;
   void setError( const QString& aError );

   LibraryController* FLibrary;
   GamesController* FGames;
   int FIndex = -1;
   GameFields FFields;
   QString FError;
};
