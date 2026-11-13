#pragma once

#include "Game.h"
#include "Result.h"

#include <functional>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>

class GamesController;
class LibraryController;
class QThread;

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
   Q_PROPERTY( QUrl videoSource READ videoSource NOTIFY mediaChanged )
   Q_PROPERTY( QVariantMap info READ info NOTIFY infoChanged )
   Q_PROPERTY( bool hashing READ isHashing NOTIFY infoChanged )

public:
   GameEditController( LibraryController* aLibrary, GamesController* aGames, QObject* aParent = nullptr );
   ~GameEditController() override;

   // The 12 editable fields by key (name, genre, rating, players, developer, publisher,
   // releaseDate, description, region, kidGame, hidden, favorite) plus the read-only romPath.
   QVariantMap values() const;
   bool isDirty() const;
   const QString& error() const { return FError; }
   // A file URL, empty without a picture. The query changes whenever the file may have, so an
   // Image showing the same path reloads.
   QString imageSource() const;
   bool hasVideo() const;
   // File URL of the video, empty when there is none.
   QUrl videoSource() const;
   // Read-only details as display text by key: gamelist, rom, image, video (a missing file says so),
   // playcount, lastPlayed, crc32, md5, sha1. Hashes are empty until computeHashes().
   QVariantMap info() const;
   bool isHashing() const { return FHashThread != nullptr; }

   // Off the UI thread; infoChanged() follows. Does nothing while hashing or when the ROM is gone.
   Q_INVOKABLE void computeHashes();

   // Unknown keys are ignored.
   Q_INVOKABLE void setField( const QString& aKey, const QVariant& aValue );
   // False (with error()) when the write failed; the typed values stay.
   Q_INVOKABLE bool save();
   // Drops the typed values.
   Q_INVOKABLE void revert();
   // Typed values survive both. False (with error()) when the write failed.
   Q_INVOKABLE bool removeImage();
   Q_INVOKABLE bool removeVideo();
   // Copy a file in (a picture is re-encoded to PNG). Same failure rules as the removals.
   Q_INVOKABLE bool changeImage( const QString& aPath );
   Q_INVOKABLE bool changeVideo( const QString& aPath );
   // Codec name ("AV1", "H.264", ...) of the video, empty when unknown.
   Q_INVOKABLE QString videoCodec() const;
   // Hands the video to the system's player; false when nothing took it.
   Q_INVOKABLE bool openVideoExternally() const;
   // Replaces the default (QDesktopServices) for opening a file, e.g. where file:// is not allowed.
   void setOpener( std::function<bool( const QString& )> aOpener ) { FOpener = std::move( aOpener ); }

signals:
   void valuesChanged();
   void dirtyChanged();
   void errorChanged();
   void mediaChanged();
   void infoChanged();

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
   int FMediaVersion = 0;
   QThread* FHashThread = nullptr;
   std::function<bool( const QString& )> FOpener;
};
