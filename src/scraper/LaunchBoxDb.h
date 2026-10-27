#pragma once

#include "Result.h"
#include "Scraper.h"

#include <QHash>
#include <QIODevice>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

// Offline lookup in LaunchBox's metadata dump (Metadata.xml, Files.xml, Mame.xml), for one
// platform at a time. Reads plain XML from a seekable device; downloading and unzipping are not here.

// Both sides of a name match go through this: tags dropped, accents and punctuation folded.
QString launchBoxNameKey( const QString& aName );

struct LaunchBoxQuery
{
   QString romName;   // file name without extension, tags kept
   QString gameName;  // Game::name
   QString region;    // Game::region
   QSet<QString> mediaTypes;  // Cst_Media* values, as for parseScrapeResponse
};

struct LaunchBoxMatch
{
   ScrapedInfo info;
   bool ambiguous = false;  // several games share the name key; the first was taken
};

struct LaunchBoxCandidate
{
   int index = -1;  // for LaunchBoxDb::infoAt
   QString name;
   QString year;     // empty when unknown
   QString altName;  // set when an alternate name matched better than the name
   double score = 0;  // 0..1
};

class LaunchBoxDb
{
public:
   // Replaces the games. Two passes, so aDevice must be seekable.
   Result<> loadMetadata( QIODevice& aDevice, const QString& aPlatform );
   // Arcade only: ROM file name to the title it is known by.
   Result<> loadFiles( QIODevice& aDevice, const QString& aPlatform );
   Result<> loadMame( QIODevice& aDevice );

   int gameCount() const { return static_cast<int>( FGames.size() ); }

   // An Error "Game not found" when nothing matches.
   Result<LaunchBoxMatch> find( const LaunchBoxQuery& aQuery ) const;

   // Games whose name or alternate name looks like aText, best first, for when find() fails.
   QVector<LaunchBoxCandidate> similar( const QString& aText, int aLimit = 20 ) const;

   // What find() returns for a game picked from similar().
   ScrapedInfo infoAt( int aIndex, const LaunchBoxQuery& aQuery ) const
   {
      return toInfo( FGames.at( aIndex ), aQuery );
   }

private:
   struct Alt
   {
      QString name;
      QString region;
   };
   struct Image
   {
      QString fileName;
      QString type;
      QString region;
   };
   struct Entry
   {
      QString name, releaseDate, releaseYear, overview, maxPlayers, genres, developer, publisher,
         rating;
      QVector<Alt> alts;
      QVector<Image> images;
   };

   ScrapedInfo toInfo( const Entry& aGame, const LaunchBoxQuery& aQuery ) const;

   QVector<Entry> FGames;
   QHash<QString, QVector<int>> FByKey;     // name key -> games, document order
   QHash<QString, QVector<int>> FByAltKey;  // alternate name key -> games
   QHash<QString, QString> FFiles;          // lower-case ROM name -> title
   QHash<QString, QString> FMame;           // lower-case MAME short name -> title
};
