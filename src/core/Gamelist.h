#pragma once

#include "Game.h"
#include "Resources.h"
#include "Result.h"
#include "Systems.h"

#include <QDomDocument>
#include <QHash>
#include <QImage>
#include <QString>
#include <QStringList>
#include <QVector>

// One system's gamelist.xml held open as a DOM. Each Game is stored with its
// <game> element so the two cannot drift apart.

// A file in the media folder that the game does not link to.
struct MediaLink
{
   int index = -1;  // the game
   bool isImage = true;
   QString path;  // absolute path of the file found
};

// A linked file that does not carry its ROM's name. A non-empty problem means
// it must not be renamed.
struct MediaRename
{
   int index = -1;  // the game
   bool isImage = true;
   QString path;     // absolute path as it is now
   QString newPath;  // absolute path it should have, in the same folder
   QString problem;
};

class Gamelist
{
public:
   // An Error when the file cannot be read or parsed, or holds no <game>.
   Result<> load( const SystemEntry& aSystem );

   const SystemEntry& system() const { return FSystem; }

   void setSystemKind( SystemKind aKind ) { FSystem.kind = aKind; }
   const QString& systemDir() const { return FSystemDir; }

   // int, not qsizetype: indices are int throughout, like QAbstractItemModel's rows.
   int count() const { return static_cast<int>( FEntries.size() ); }
   const Game& at( int aIndex ) const { return FEntries.at( aIndex ).game; }

   Result<> save();

   void setFields( const QVector<int>& aTargets, const GameFields& aValues, GameFieldSet aWhich );

   Result<> setImage( int aIndex, const QString& aSourcePath );
   Result<> setImage( int aIndex, const QImage& aPicture );
   Result<> setVideo( int aIndex, const QString& aSourcePath );

   Result<> removeImage( int aIndex );
   Result<> removeVideo( int aIndex );

   // Also deletes the ROM and media, except files another game still uses.
   // Later indices shift down. The value lists files that could not be deleted;
   // the game is gone regardless. An Error leaves everything as it was.
   Result<QStringList> removeGame( int aIndex );

   // Removes the entries only, no files. Later indices shift down.
   Result<> removeGames( const QVector<int>& aIndexes );

   // Games whose ROM is not on disk.
   QVector<int> orphanIndexes() const;

   // The later entries of each ROM listed more than once. Same file, not same name.
   QVector<int> duplicateIndexes() const;

   // Absolute paths of unlisted ROM candidates, sorted. Media is excluded.
   QStringList unlistedRoms() const;

   // Appends the new games at the end. Already-listed files are skipped; the value
   // is how many were added. An Error leaves the list as it was.
   Result<int> addGames( const QStringList& aRomPaths );

   // All or nothing. The value is how many games got the picture.
   Result<int> setDefaultImageForMissing( const QString& aSourcePath );

   // The value is how many links were set. An Error leaves everything as it was.
   Result<int> linkMedia( const QVector<MediaLink>& aLinks );

   // Skips entries with a problem. The value is how many were renamed. An Error
   // leaves list and files as they were.
   Result<int> renameMedia( const QVector<MediaRename>& aRenames );

   // Compares resolved paths, not link text.
   bool fileShared( int aIndex, const QString& aPhysicalPath, bool aIsImage ) const;

   void ensureHashes( int aIndex );

   // Child text as it is in the DOM, not the Game. Empty if absent.
   QString domText( int aIndex, const char* aName ) const;

   // Where new media goes; copied from the first game that has any.
   const QString& imageFolder() const { return FImageFolder; }
   const QString& videoFolder() const { return FVideoFolder; }


   bool nameIsDuplicated( const QString& aName ) const { return FNameCounts.value( aName ) > 1; }
   bool romIsDuplicated( const QString& aRomName ) const { return FRomCounts.value( aRomName ) > 1; }

private:

   Result<> checkIndex( int aIndex ) const;


   Game gameFromNode( const QDomElement& aNode ) const;

   QDomElement ensureChild( QDomElement aNode, const char* aName );
   void setChildText( QDomElement aNode, const char* aName, const QString& aText );

   // Undo record for one media write. For a rename, undo moves target back to
   // movedFrom instead of deleting it.
   struct MediaWrite
   {
      int index = -1;
      bool isImage = true;
      QString target;     // the file written, or the new name of a renamed one
      QString backup;     // what was there before, moved aside; empty if nothing was
      QString movedFrom;  // the old name of a renamed file; empty unless a rename
      Game before;        // the game as it was, link text included
   };

   // Does not save. On an Error nothing has changed.
   Result<MediaWrite> applyImage( int aIndex, const QImage& aPicture );

   // Never a file another game has.
   QString freeMediaLink( int aIndex, bool aIsImage ) const;

   Result<> commitMedia( const QVector<MediaWrite>& aWrites );
   // Newest first: a game written twice in one batch holds its first snapshot last.
   void rollbackMedia( const QVector<MediaWrite>& aWrites );
   void undoMedia( const MediaWrite& aWrite );
   void restoreLink( int aIndex, bool aIsImage, const Game& aBefore );

   void countDuplicates();

   SystemEntry FSystem;
   QString FSystemDir;
   QDomDocument FDocument;

   struct Entry
   {
      Game game;
      QDomElement node;
   };
   QVector<Entry> FEntries;

   QString FImageFolder;
   QString FVideoFolder;
   bool FFoldsCase = false;

   QHash<QString, int> FNameCounts;
   QHash<QString, int> FRomCounts;
};
