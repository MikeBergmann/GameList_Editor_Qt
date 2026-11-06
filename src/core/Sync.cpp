#include "Sync.h"

#include "Gamelist.h"
#include "PathUtil.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSet>

namespace {

QString romDirOf( const SystemEntry& aSystem )
{
   return aSystem.romDir.isEmpty() ? QFileInfo( aSystem.gamelistPath ).absolutePath()
                                   : QDir( aSystem.romDir ).absolutePath();
}

// Same size, then same MD5. A missing target differs.
bool mediaDiffers( const QString& aSource, const QString& aTarget )
{
   const QFileInfo target( aTarget );
   if ( !target.exists() || target.size() != QFileInfo( aSource ).size() )
      return true;
   return fileHash( aSource, QCryptographicHash::Md5 ) != fileHash( aTarget, QCryptographicHash::Md5 );
}

struct MediaVerdict
{
   bool copy = false;
   QString problem;
};

// aTargetGame is null for a game the target does not have yet.
MediaVerdict judgeMedia( const Game& aSource, const Game* aTargetGame, bool aIsImage, bool aTargetEsde )
{
   const bool sourceMissing = aIsImage ? aSource.missingImage : aSource.missingVideo;
   if ( sourceMissing )
      return {};

   const QString& source = aIsImage ? aSource.physicalImagePath : aSource.physicalVideoPath;
   const QString suffix = QFileInfo( source ).suffix();
   const bool unreadable = aTargetEsde && esdeMediaSuffix( aIsImage, suffix ).isEmpty();

   // A video ES-DE cannot play is left out; a picture is converted on the way in.
   if ( unreadable && !aIsImage )
      return { false, QStringLiteral( "%1: ES-DE cannot play the .%2 video." ).arg( aSource.name, suffix ) };

   const bool targetMissing = !aTargetGame || ( aIsImage ? aTargetGame->missingImage : aTargetGame->missingVideo );
   if ( targetMissing )
      return { true, {} };

   // ponytail: a converted picture can no longer be compared with its source, so it is copied
   // only when absent. Upgrade: remember the source hash on the target.
   if ( unreadable )
      return {};

   return { mediaDiffers( source, aIsImage ? aTargetGame->physicalImagePath : aTargetGame->physicalVideoPath ), {} };
}

}  // namespace

GameFieldSet syncedFields()
{
   return gfName | gfGenre | gfRating | gfPlayers | gfDeveloper | gfPublisher | gfReleaseDate | gfDescription |
          gfRegion;
}

QVector<SystemPair> pairSystems( const QVector<SystemEntry>& aMaster, const QString& aTargetRoot )
{
   QVector<SystemEntry> candidates = scanSystems( aTargetRoot );
   applySystemMap( candidates );
   QVector<bool> taken( candidates.size(), false );

   const auto claim = [&]( const auto& aMatches ) -> int {
      for ( int i = 0; i < candidates.size(); ++i ) {
         if ( !taken[i] && aMatches( candidates.at( i ) ) ) {
            taken[i] = true;
            return i;
         }
      }
      return -1;
   };

   QVector<SystemPair> pairs;
   for ( const SystemEntry& source : aMaster ) {
      int found = claim( [&]( const SystemEntry& aTarget ) { return aTarget.folderName == source.folderName; } );
      if ( found < 0 && source.kind != skOther )
         found = claim( [&]( const SystemEntry& aTarget ) { return aTarget.kind == source.kind; } );

      SystemPair pair;
      pair.source = source;
      pair.targetExists = found >= 0;
      pair.target = found >= 0 ? candidates.at( found ) : systemAt( aTargetRoot, source.folderName );
      pairs.append( pair );
   }
   return pairs;
}

SystemPlan planSystem( const SystemPair& aPair )
{
   SystemPlan plan;
   plan.pair = aPair;

   const QString sourceFile = QFileInfo( aPair.source.gamelistPath ).canonicalFilePath();
   if ( !sourceFile.isEmpty() && sourceFile == QFileInfo( aPair.target.gamelistPath ).canonicalFilePath() ) {
      plan.problems << QStringLiteral( "%1: the target is the master's own gamelist." ).arg( aPair.source.folderName );
      return plan;
   }

   Gamelist master;
   if ( const Result<> loaded = master.load( aPair.source, true ); !loaded ) {
      plan.problems << loaded.error();
      return plan;
   }

   Gamelist target;  // stays empty when the target has no gamelist yet
   if ( aPair.targetExists ) {
      if ( const Result<> loaded = target.load( aPair.target, true ); !loaded ) {
         plan.problems << loaded.error();
         return plan;
      }
   }

   const bool foldsCase = target.foldsCase();
   const bool targetEsde = !aPair.target.mediaDir.isEmpty();
   const QString targetRomDir = romDirOf( aPair.target );
   plan.copyRoms = !sameFolder( master.romDir(), targetRomDir );

   const auto keyOf = [foldsCase]( const Game& aGame ) {
      return fileKey( QDir::cleanPath( aGame.romPath ), foldsCase );
   };

   QHash<QString, int> targetKeys;
   for ( int i = 0; i < target.count(); ++i )
      targetKeys.insert( keyOf( target.at( i ) ), i );  // a duplicate on the target: the last one wins

   QSet<int> matched;
   QSet<QString> seen;
   const GameFieldSet mask = syncedFields();

   for ( int i = 0; i < master.count(); ++i ) {
      const Game& source = master.at( i );

      if ( QDir::isAbsolutePath( source.romPath ) ) {
         plan.problems << QStringLiteral( "%1: the path %2 is absolute and cannot be matched." )
                             .arg( source.name, source.romPath );
         continue;
      }

      const QString key = keyOf( source );
      if ( seen.contains( key ) ) {
         plan.problems << QStringLiteral( "%1: %2 is listed more than once on the master; the first is used." )
                             .arg( source.name, source.romPath );
         continue;
      }
      seen.insert( key );

      const int targetIndex = targetKeys.value( key, -1 );
      if ( targetIndex >= 0 )
         matched.insert( targetIndex );

      if ( source.isOrphan && targetIndex < 0 ) {
         plan.problems << QStringLiteral( "%1: the ROM %2 is missing on the master." ).arg( source.name, source.romPath );
         continue;
      }

      SyncGame game;
      game.sourceIndex = i;
      game.targetIndex = targetIndex;
      game.name = source.name;
      game.romLink = source.romPath;

      const GameFields values = gameFields( source );
      game.fields = ( targetIndex < 0 ? filledFields( values ) : changedFields( target.at( targetIndex ), values ) ) & mask;

      if ( plan.copyRoms && !source.isOrphan ) {
         game.rom = planRomCopy( master.romDir(), targetRomDir, source.romPath );
         plan.problems << game.rom.problems;
         plan.romBytes += game.rom.bytes;
      }

      const Game* existing = targetIndex < 0 ? nullptr : &target.at( targetIndex );
      for ( const bool isImage : { true, false } ) {
         const MediaVerdict verdict = judgeMedia( source, existing, isImage, targetEsde );
         if ( !verdict.problem.isEmpty() )
            plan.problems << verdict.problem;
         if ( !verdict.copy )
            continue;
         if ( isImage ) {
            game.copyImage = true;
            game.imageSource = source.physicalImagePath;
         } else {
            game.copyVideo = true;
            game.videoSource = source.physicalVideoPath;
         }
      }

      if ( targetIndex < 0 || game.fields || !game.rom.files.isEmpty() || game.copyImage || game.copyVideo )
         plan.games.append( game );
      else
         ++plan.unchanged;
   }

   for ( int i = 0; i < target.count(); ++i ) {
      if ( !matched.contains( i ) )
         plan.targetOnly << target.at( i ).name;
   }

   plan.createTarget = !aPair.targetExists && !plan.games.isEmpty();
   return plan;
}

bool SyncPlan::isEmpty() const
{
   for ( const SystemPlan& system : systems ) {
      if ( !system.isEmpty() )
         return false;
   }
   return true;
}

SyncPlan planSync( const QVector<SystemEntry>& aMaster, const QString& aTargetRoot )
{
   SyncPlan plan;
   for ( const SystemPair& pair : pairSystems( aMaster, aTargetRoot ) ) {
      SystemPlan system = planSystem( pair );
      if ( !system.isEmpty() || !system.problems.isEmpty() || !system.targetOnly.isEmpty() )
         plan.systems.append( std::move( system ) );
   }
   return plan;
}

SystemReport applySystem( const SystemPlan& aPlan, const SyncProgress& aProgress )
{
   SystemReport report;
   report.name = aPlan.pair.source.folderName;
   if ( aPlan.games.isEmpty() )
      return report;

   const auto fail = [&report]( const QString& aMessage ) {
      report.error = aMessage;
      return report;
   };
   const auto proceed = [&]( const QString& aMessage ) {
      if ( aProgress && !aProgress( aMessage ) ) {
         report.canceled = true;
         return false;
      }
      return true;
   };

   Gamelist master;
   if ( const Result<> loaded = master.load( aPlan.pair.source, true ); !loaded )
      return fail( loaded.error() );

   if ( aPlan.copyRoms ) {
      QVector<RomFile> files;
      for ( const SyncGame& game : aPlan.games )
         files += game.rom.files;

      if ( !files.isEmpty() ) {
         if ( const Result<> space = checkFreeSpace( romDirOf( aPlan.pair.target ), aPlan.romBytes ); !space )
            return fail( space.error() );

         const Result<> copied = copyRomFiles( files, [&]( const RomFile& aFile ) {
            return proceed( QStringLiteral( "Copying %1" ).arg( QFileInfo( aFile.source ).fileName() ) );
         } );
         if ( !copied )
            return fail( report.canceled ? QStringLiteral( "Canceled." ) : copied.error() );
      }
   }

   if ( !proceed( QStringLiteral( "Writing %1" ).arg( report.name ) ) )
      return fail( QStringLiteral( "Canceled." ) );

   if ( aPlan.createTarget ) {
      if ( const Result<> made = createGamelist( aPlan.pair.target.gamelistPath ); !made )
         return fail( made.error() );
   }

   Gamelist target;
   if ( const Result<> loaded = target.load( aPlan.pair.target, true ); !loaded )
      return fail( loaded.error() );

   const auto keyOf = [&target]( const Game& aGame ) {
      return fileKey( QDir::cleanPath( aGame.romPath ), target.foldsCase() );
   };

   for ( const SyncGame& game : aPlan.games ) {
      if ( game.targetIndex < 0 )
         continue;
      if ( game.targetIndex >= target.count() || keyOf( target.at( game.targetIndex ) ) != keyOf( master.at( game.sourceIndex ) ) )
         return fail( QStringLiteral( "%1 changed since the plan was made. Plan again." ).arg( report.name ) );
   }

   // A ROM that did not reach the target (missing on the master) cannot become a game.
   QStringList romPaths;
   QSet<int> unplaced;
   for ( int i = 0; i < aPlan.games.size(); ++i ) {
      const SyncGame& game = aPlan.games.at( i );
      if ( game.targetIndex >= 0 )
         continue;
      const QString path = QDir( target.romDir() ).filePath( QDir::cleanPath( game.romLink ) );
      if ( QFileInfo( path ).isFile() )
         romPaths << path;
      else {
         unplaced.insert( i );
         report.skipped << QStringLiteral( "%1: the ROM is not on the target." ).arg( game.name );
      }
   }

   if ( !romPaths.isEmpty() ) {
      const Result<int> added = target.addGames( romPaths );
      if ( !added )
         return fail( added.error() );
      report.added = added.value();
   }

   QHash<QString, int> targetKeys;
   for ( int i = 0; i < target.count(); ++i )
      targetKeys.insert( keyOf( target.at( i ) ), i );

   QVector<FieldUpdate> updates;
   QVector<MediaCopy> copies;
   for ( int i = 0; i < aPlan.games.size(); ++i ) {
      const SyncGame& game = aPlan.games.at( i );
      if ( unplaced.contains( i ) )
         continue;

      const Game& source = master.at( game.sourceIndex );
      const int index = game.targetIndex >= 0 ? game.targetIndex : targetKeys.value( keyOf( source ), -1 );
      if ( index < 0 ) {
         report.skipped << QStringLiteral( "%1: not found on the target after adding." ).arg( game.name );
         continue;
      }

      if ( game.targetIndex >= 0 )
         ++report.updated;
      if ( game.fields )
         updates.append( { index, gameFields( source ), game.fields } );
      if ( game.copyImage )
         copies.append( { index, true, game.imageSource } );
      if ( game.copyVideo )
         copies.append( { index, false, game.videoSource } );
   }

   if ( !updates.isEmpty() ) {
      if ( const Result<> set = target.setFields( updates ); !set )
         return fail( set.error() );
   }

   if ( !copies.isEmpty() ) {
      const Result<MediaCopyReport> copied = target.copyMedia( copies );
      if ( !copied )
         return fail( copied.error() );
      report.mediaCopied = copied.value().copied;
      report.skipped << copied.value().skipped;
   }
   return report;
}

SyncReport applySync( const SyncPlan& aPlan, const SyncProgress& aProgress )
{
   SyncReport report;
   for ( const SystemPlan& system : aPlan.systems ) {
      if ( system.isEmpty() )
         continue;
      report.systems.append( applySystem( system, aProgress ) );
      if ( report.systems.last().canceled ) {
         report.canceled = true;
         break;
      }
   }
   return report;
}
