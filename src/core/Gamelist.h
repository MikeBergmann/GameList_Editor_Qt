#pragma once

#include "Game.h"
#include "Resources.h"

#include <QDomDocument>
#include <QHash>
#include <QImage>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

// One system's gamelist.xml held open as a DOM, plus the root folder scan.
//
// load() parses the document once; setFields() and the media actions edit it in
// memory and save() writes it out. Games and their <game> elements are
// index-aligned (see gameNode()).

// One system folder under the ROM root that holds a gamelist.xml.
struct SystemEntry
{
   QString folderName;
   SystemKind kind = skOther;
   QString gamelistPath;
};

QVector<SystemEntry> scanSystems( const QString& aRootPath );

Q_DECLARE_METATYPE( SystemEntry )

QString resolvePath( const QString& aPath );

// The file aRelative (a link as the gamelist writes it) names under aSystemDir, with
// its case settled against the disk. Empty for an empty link.
QString physicalPath( const QString& aSystemDir, const QString& aRelative );

QString gamelistDateToDisplay( const QString& aValue );
QString displayDateToGamelist( const QString& aDate );

// A picture or video that sits in the media folder under the ROM's name but that
// the <game> does not point at (no link, or a link to a file that is not there).
// Found by unlinkedMedia(), applied by Gamelist::linkMedia.
struct MediaLink
{
   int index = -1;  // the game
   bool isImage = true;
   QString path;  // absolute path of the file found
};

// A linked picture or video that does not carry its ROM's name. A non-empty
// problem means it must not be renamed (and why), so the review can show it.
// Found by misnamedMedia(), applied by Gamelist::renameMedia.
struct MediaRename
{
   int index = -1;  // the game
   bool isImage = true;
   QString path;     // absolute path as it is now
   QString newPath;  // absolute path it should have, in the same folder
   QString problem;
};

// What the filter combo offers. The enumerators are in the order the combo lists them.
// SameRegion..SameRom are "same as the selected game", and must stay contiguous:
// filterDependsOnSelection() is a range check.
enum class FilterCategory
{
   All,
   MissingImage,
   MissingVideo,
   MissingDate,
   MissingPlayers,
   MissingRating,
   MissingDeveloper,
   MissingPublisher,
   MissingDescription,
   MissingGenre,
   MissingRegion,
   KidGame,
   Hidden,
   Favorite,
   Orphan,
   SameRegion,
   SameDate,
   SameOrEarlierDate,
   SameOrLaterDate,
   SamePlayers,
   SameRating,
   SameOrLowerRating,
   SameOrHigherRating,
   SamePublisher,
   SameDeveloper,
   SameGenre,
   SameFolder,
   SameName,
   SameRom,
   DuplicateNames,
   DuplicateRoms,
   Count,  // not a category: the number of them
};

// Whether the answer depends on which game is selected, so a view must re-run the
// filter when the selection changes.
inline bool filterDependsOnSelection( FilterCategory aCategory )
{
   return aCategory >= FilterCategory::SameRegion && aCategory <= FilterCategory::SameRom;
}

struct FilterSpec
{
   FilterCategory category = FilterCategory::All;
   QString search;
   bool listByRom = false;
   bool fullRomName = false;

   // Index of the selected game. The "same as" categories compare against it, so a
   // view must re-run the filter when the selection changes.
   int reference = -1;
};

class Gamelist
{
public:
   // Parses aSystem's gamelist.xml and keeps the document open. False (with a
   // message in aError, if given) when the file cannot be read or parsed, or
   // holds no <game> element.
   bool load( const SystemEntry& aSystem, QString* aError = nullptr );

   const SystemEntry& system() const { return FSystem; }
   // Re-types the open system (a user mapping); nothing on disk depends on it.
   void setSystemKind( SystemKind aKind ) { FSystem.kind = aKind; }
   const QString& systemDir() const { return FSystemDir; }
   const QVector<Game>& games() const { return FGames; }
   // int, not qsizetype: indices and rows are int throughout (QAbstractItemModel's
   // are too), and a gamelist is nowhere near INT_MAX entries.
   int count() const { return static_cast<int>( FGames.size() ); }
   const Game& at( int aIndex ) const { return FGames.at( aIndex ); }

   bool save( QString* aError = nullptr );

   void setFields( const QVector<int>& aTargets, const GameFields& aValues, GameFieldSet aWhich );

   bool setImage( int aIndex, const QString& aSourcePath, QString* aError = nullptr );
   bool setImage( int aIndex, const QImage& aPicture, QString* aError = nullptr );
   bool setVideo( int aIndex, const QString& aSourcePath, QString* aError = nullptr );

   bool removeImage( int aIndex, QString* aError = nullptr );
   bool removeVideo( int aIndex, QString* aError = nullptr );

   // God mode's delete: drops the <game> and indices after aIndex shift down, so
   // callers must reset their views. The ROM, picture and video go too, except
   // any that another game still points at. False, with the gamelist untouched, if
   // the write failed; true with aError set if only some file could not be removed.
   bool removeGame( int aIndex, QString* aError = nullptr );

   // Actions > System > Delete orphans / duplicates: drops the <game>s only, no
   // file, then writes the gamelist once. Indices shift afterwards, so callers
   // must reset their views. False, with the gamelist untouched, if the write failed.
   bool removeGames( const QVector<int>& aIndexes, QString* aError = nullptr );

   // Games whose ROM is not on disk.
   QVector<int> orphanIndexes() const;

   // The later entries of every ROM listed more than once, so the first stays.
   // The same file is meant: Sonic.zip in two folders is two games.
   QVector<int> duplicateIndexes() const;

   // Files under the system folder that no <game> points at: the candidates for
   // addGames. Absolute paths, sorted. Media and other non-ROM files are left out.
   QStringList unlistedRoms() const;

   // Appends a <game> (path and name only) per file, then writes the gamelist
   // once. New games take the indices count()..count()+n-1. Files already listed
   // are skipped. Returns how many were added, or -1 with aError set and the
   // gamelist untouched: a file outside the system folder, or a failed write.
   int addGames( const QStringList& aRomPaths, QString* aError = nullptr );

   int setDefaultImageForMissing( const QString& aSourcePath, QString* aError = nullptr );

   // Points the games at the files, in place, then writes the gamelist once.
   // Returns how many links were set, or -1 with aError set and the gamelist
   // as it was: a failed write.
   int linkMedia( const QVector<MediaLink>& aLinks, QString* aError = nullptr );

   // Renames the files without a problem and repoints the games at them, then
   // writes the gamelist once. Returns how many were renamed, or -1 with aError
   // set, the gamelist as it was and the files back under their old names.
   int renameMedia( const QVector<MediaRename>& aRenames, QString* aError = nullptr );

   // Whether a game other than aIndex resolves to the same file on disk. Compared
   // by resolved path, not link text: links differing only in case are one file.
   bool fileShared( int aIndex, const QString& aPhysicalPath, bool aIsImage ) const;

   void ensureHashes( int aIndex );

   // The <game> element aIndex was parsed from, for the save paths. QDomElement
   // is a handle into FDocument, so this is a lookup, not a search.
   QDomElement gameNode( int aIndex ) const { return FNodes.value( aIndex ); }

   // The folder new media goes into, taken at load from the first game that has
   // any, else ./media/images/ and ./media/videos/.
   const QString& imageFolder() const { return FImageFolder; }
   const QString& videoFolder() const { return FVideoFolder; }

   // The filter predicate, category and search text together.
   bool accepts( int aIndex, const FilterSpec& aFilter ) const;

   // What the list shows for that game, and what the search text matches.
   QString displayName( int aIndex, const FilterSpec& aFilter ) const;

private:
   // One <game> element as a Game, filesystem flags included.
   Game gameFromNode( const QDomElement& aNode ) const;
   bool matchesCategory( const Game& aGame, const FilterSpec& aFilter ) const;

   QDomElement ensureChild( QDomElement aNode, const char* aName );
   void setChildText( QDomElement aNode, const char* aName, const QString& aText );

   // What one media write changed, so a failed save can put it all back. target and
   // backup are empty when no file was touched (removals, a video copied onto itself).
   struct MediaWrite
   {
      int index = -1;
      bool isImage = true;
      QString target;  // the file written
      QString backup;  // what was there before, moved aside; empty if nothing was
      Game before;     // the game as it was, link text included
   };

   // Writes the picture and edits DOM and game, without saving. aWrite is only valid
   // when this returns true; on false nothing has changed.
   bool applyImage( int aIndex, const QImage& aPicture, MediaWrite& aWrite, QString* aError );
   // Saves; on success drops the backups, on failure undoes aWrites (files, DOM, games).
   bool commitMedia( const QVector<MediaWrite>& aWrites, QString* aError );
   void undoMedia( const MediaWrite& aWrite );
   void restoreLink( int aIndex, bool aIsImage, const Game& aBefore );

   void countDuplicates();

   SystemEntry FSystem;
   QString FSystemDir;
   QDomDocument FDocument;
   QVector<Game> FGames;
   QVector<QDomElement> FNodes;

   QString FImageFolder;
   QString FVideoFolder;

   QHash<QString, int> FNameCounts;
   QHash<QString, int> FRomCounts;
};
