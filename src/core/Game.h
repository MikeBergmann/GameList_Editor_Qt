#pragma once

#include <QCryptographicHash>
#include <QDateTime>
#include <QFileInfo>
#include <QString>

// One <game> node of gamelist.xml. Plain data: the XML layer fills it in.
struct Game
{
   // Path exactly as stored in the gamelist, e.g. "./Sonic The Hedgehog 2.zip".
   QString romPath;
   QString name;
   QString description;
   QString imagePath;
   QString videoPath;
   QString rating;
   QString releaseDate;
   QString developer;
   QString publisher;
   QString genre;
   QString players;
   QString region;
   QString playcount;
   QString lastplayed;
   QString crc32;
   QString md5;
   QString sha1;

   // 0/1, not bool: these are bound straight to a two-item QComboBox's
   // currentIndex, and batch edits use -1 to mean "leave unchanged".
   int kidGame = 0;
   int hidden = 0;
   int favorite = 0;

   // Filesystem state, snapshotted when the gamelist is parsed. The filters
   // that use these run on every keystroke, so they must not stat per row.
   bool isOrphan = false;
   bool missingImage = false;
   bool missingVideo = false;

   // romPath/imagePath/videoPath resolved against the ROM folder. May not exist.
   QString physicalRomPath;
   QString physicalImagePath;
   QString physicalVideoPath;

   QString romName() const { return QFileInfo( romPath ).fileName(); }
   QString romNameWoExt() const { return QFileInfo( romPath ).completeBaseName(); }
};

// The 12 editable fields, as one value.
//
// This is the seam between the UI and the data.
struct GameFields
{
   QString name;
   QString genre;
   QString rating;
   QString players;
   QString developer;
   QString publisher;
   QString releaseDate;
   QString description;
   QString region;

   // -1 is "not set"
   int kidGame = -1;
   int hidden = -1;
   int favorite = -1;
};

enum GameField
{
   gfName = 1 << 0,
   gfGenre = 1 << 1,
   gfRating = 1 << 2,
   gfPlayers = 1 << 3,
   gfDeveloper = 1 << 4,
   gfPublisher = 1 << 5,
   gfReleaseDate = 1 << 6,
   gfDescription = 1 << 7,
   gfRegion = 1 << 8,
   gfKidGame = 1 << 9,
   gfHidden = 1 << 10,
   gfFavorite = 1 << 11,
};
Q_DECLARE_FLAGS( GameFieldSet, GameField )
Q_DECLARE_OPERATORS_FOR_FLAGS( GameFieldSet )

// The current values of a game, for loading the editor.
GameFields gameFields( const Game& aGame );

GameFieldSet changedFields( const Game& aGame, const GameFields& aValues );

GameFieldSet filledFields( const GameFields& aValues );

// gamelist.xml stores booleans as the literal "true"; anything else is false.
int gamelistFlag( const QString& aValue );

// <lastplayed> and <releasedate> are basic ISO 8601 with no zone designator,
// e.g. "20130101T180000". EmulationStation writes local time, so it is read
// back as local time. Invalid or empty input gives an invalid QDateTime.
QDateTime gamelistDateTime( const QString& aValue );

// Uppercase hex to match the values Indy's HashStreamAsHex already wrote into
// existing gamelists. Empty if the file cannot be read.
QString fileHash( const QString& aPath, QCryptographicHash::Algorithm aAlgorithm );
QString fileCrc32( const QString& aPath );
