#include "Sync.h"

#include "Gamelist.h"
#include "PathUtil.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSet>

namespace {

// By the file's case on disk, so a Windows link matches the file a sync copied.
QString romKey( const Gamelist& aList, const QString& aPhysicalPath, bool aFoldsCase )
{
   return fileKey( QDir( aList.romDir() ).relativeFilePath( aPhysicalPath ), aFoldsCase );
}

// A duplicate on the list: the last one wins.
QHash<QString, int> romIndex( const Gamelist& aList, bool aFoldsCase )
{
   QHash<QString, int> keys;
   for ( int i = 0; i < aList.count(); ++i )
      keys.insert( romKey( aList, aList.at( i ).physicalRomPath, aFoldsCase ), i );
   return keys;
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

// Why a master game stays off the plan; empty when it can go on.
QString rejection( const Game& aSource, bool aDuplicate, bool aOnTarget )
{
   if ( QDir::isAbsolutePath( aSource.romPath ) )
      return QStringLiteral( "%1: the path %2 is absolute and cannot be matched." ).arg( aSource.name, aSource.romPath );

   if ( aDuplicate )
      return QStringLiteral( "%1: %2 is listed more than once on the master; the first is used." )
         .arg( aSource.name, aSource.romPath );

   if ( aOnTarget )
      return {};

   if ( aSource.isOrphan )
      return QStringLiteral( "%1: the ROM %2 is missing on the master." ).arg( aSource.name, aSource.romPath );

   // ponytail: addGames takes files only. Upgrade: let it add a folder ROM.
   if ( QFileInfo( aSource.physicalRomPath ).isDir() )
      return QStringLiteral( "%1: %2 is a folder ROM; folder ROMs are not synced." ).arg( aSource.name, aSource.romPath );

   return {};
}

// What one master game needs on the target (aTargetIndex -1: it is new there). The problems
// of its ROM files and media, and the ROM bytes, are added to aPlan.
SyncGame planGame( SystemPlan& aPlan, const Gamelist& aMaster, const Gamelist& aTarget, int aSourceIndex,
                   int aTargetIndex )
{
   const Game& source = aMaster.at( aSourceIndex );
   SyncGame game;
   game.sourceIndex = aSourceIndex;
   game.targetIndex = aTargetIndex;
   game.name = source.name;
   game.romLink = source.romPath;

   const GameFields values = gameFields( source );
   game.fields = ( aTargetIndex < 0 ? filledFields( values ) : changedFields( aTarget.at( aTargetIndex ), values ) ) &
                 syncedFields();

   if ( aPlan.copyRoms && !source.isOrphan ) {
      game.rom = planRomCopy( aMaster.romDir(), romDirOf( aPlan.pair.target ), source.romPath );
      aPlan.problems << game.rom.problems;
      aPlan.romBytes += game.rom.bytes;
   }

   const Game* existing = aTargetIndex < 0 ? nullptr : &aTarget.at( aTargetIndex );
   const bool targetEsde = !aPlan.pair.target.mediaDir.isEmpty();
   for ( const bool isImage : { true, false } ) {
      const MediaVerdict verdict = judgeMedia( source, existing, isImage, targetEsde );
      if ( !verdict.problem.isEmpty() )
         aPlan.problems << verdict.problem;
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
   return game;
}

using Proceed = std::function<bool( const QString& )>;

Result<> checkMasterUnchanged( const SystemPlan& aPlan, const Gamelist& aMaster )
{
   for ( const SyncGame& game : aPlan.games ) {
      if ( game.sourceIndex < 0 || game.sourceIndex >= aMaster.count() ||
           aMaster.at( game.sourceIndex ).romPath != game.romLink )
         return Error{ QStringLiteral( "The master's %1 changed since the plan was made. Plan again." )
                          .arg( aPlan.pair.source.folderName ) };
   }
   return {};
}

// Loads the target and checks that every game the plan matched is still the same ROM.
Result<> loadTarget( const SystemPlan& aPlan, const Gamelist& aMaster, Gamelist& aTarget )
{
   if ( const Result<> loaded = aTarget.load( aPlan.pair.target, true ); !loaded )
      return loaded;

   const bool foldsCase = aTarget.foldsCase();
   for ( const SyncGame& game : aPlan.games ) {
      if ( game.targetIndex < 0 )
         continue;
      if ( game.targetIndex >= aTarget.count() ||
           romKey( aTarget, aTarget.at( game.targetIndex ).physicalRomPath, foldsCase ) !=
              romKey( aMaster, aMaster.at( game.sourceIndex ).physicalRomPath, foldsCase ) )
         return Error{ QStringLiteral( "%1 changed since the plan was made. Plan again." )
                          .arg( aPlan.pair.source.folderName ) };
   }
   return {};
}

Result<> copyPlannedRoms( const SystemPlan& aPlan, const Proceed& aProceed )
{
   QVector<RomFile> files;
   for ( const SyncGame& game : aPlan.games )
      files += game.rom.files;
   if ( !aPlan.copyRoms || files.isEmpty() )
      return {};

   if ( const Result<> space = checkFreeSpace( romDirOf( aPlan.pair.target ), aPlan.romBytes ); !space )
      return space;

   return copyRomFiles( files, [&]( const RomFile& aFile ) {
      return aProceed( QStringLiteral( "%1: copying %2" )
                          .arg( aPlan.pair.source.folderName, QFileInfo( aFile.source ).fileName() ) );
   } );
}

// Puts the plan's new games on the target. Returns the key of each ROM given to addGames,
// by plan row. A ROM that did not reach the target (missing on the master) cannot become a game.
Result<QHash<int, QString>> addNewGames( const SystemPlan& aPlan, Gamelist& aTarget, SystemReport& aReport )
{
   QStringList romPaths;
   QHash<int, QString> addedKeys;
   for ( int i = 0; i < aPlan.games.size(); ++i ) {
      const SyncGame& game = aPlan.games.at( i );
      if ( game.targetIndex >= 0 )
         continue;
      const QString path = physicalPath( aTarget.romDir(), game.romLink );
      if ( QFileInfo( path ).isFile() ) {
         romPaths << path;
         addedKeys.insert( i, romKey( aTarget, path, aTarget.foldsCase() ) );
      } else {
         aReport.skipped << QStringLiteral( "%1: the ROM is not on the target." ).arg( game.name );
      }
   }

   if ( !romPaths.isEmpty() ) {
      const Result<int> added = aTarget.addGames( romPaths );
      if ( !added )
         return added;
      aReport.added = added.value();
   }
   return addedKeys;
}

struct TargetChanges
{
   QVector<FieldUpdate> updates;
   QVector<MediaCopy> copies;
};

// Resolves each plan row to its target index (after addGames, so the indices are fresh).
TargetChanges collectChanges( const SystemPlan& aPlan, const Gamelist& aMaster, const Gamelist& aTarget,
                              const QHash<int, QString>& aAddedKeys, SystemReport& aReport )
{
   const QHash<QString, int> targetKeys = romIndex( aTarget, aTarget.foldsCase() );
   TargetChanges changes;

   for ( int i = 0; i < aPlan.games.size(); ++i ) {
      const SyncGame& game = aPlan.games.at( i );
      if ( game.targetIndex < 0 && !aAddedKeys.contains( i ) )
         continue;

      const int index = game.targetIndex >= 0 ? game.targetIndex : targetKeys.value( aAddedKeys.value( i ), -1 );
      if ( index < 0 ) {
         aReport.skipped << QStringLiteral( "%1: not found on the target after adding." ).arg( game.name );
         continue;
      }

      if ( game.targetIndex >= 0 )
         ++aReport.updated;
      if ( game.fields )
         changes.updates.append( { index, gameFields( aMaster.at( game.sourceIndex ) ), game.fields } );
      if ( game.copyImage )
         changes.copies.append( { index, true, game.imageSource } );
      if ( game.copyVideo )
         changes.copies.append( { index, false, game.videoSource } );
   }
   return changes;
}

Result<> writeChanges( const TargetChanges& aChanges, Gamelist& aTarget, SystemReport& aReport,
                       const Proceed& aProceed )
{
   if ( !aChanges.updates.isEmpty() ) {
      if ( const Result<> set = aTarget.setFields( aChanges.updates ); !set )
         return set;
   }

   if ( !aChanges.copies.isEmpty() ) {
      const Result<MediaCopyReport> copied = aTarget.copyMedia( aChanges.copies, [&]( const MediaCopy& aCopy ) {
         return aProceed( QStringLiteral( "%1: copying %2" ).arg( aReport.name, QFileInfo( aCopy.source ).fileName() ) );
      } );
      if ( !copied )
         return copied;
      aReport.mediaCopied = copied.value().copied;
      aReport.skipped << copied.value().skipped;
   }
   return {};
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

   // All names first: a mapped folder claiming by kind must not take another's namesake.
   QVector<int> found( aMaster.size(), -1 );
   for ( int i = 0; i < aMaster.size(); ++i ) {
      const QString& folder = aMaster.at( i ).folderName;
      found[i] = claim( [&]( const SystemEntry& aTarget ) { return aTarget.folderName == folder; } );
   }
   for ( int i = 0; i < aMaster.size(); ++i ) {
      const SystemKind kind = aMaster.at( i ).kind;
      if ( found[i] < 0 && kind != skOther )
         found[i] = claim( [&]( const SystemEntry& aTarget ) { return aTarget.kind == kind; } );
   }

   QVector<SystemPair> pairs;
   for ( int i = 0; i < aMaster.size(); ++i ) {
      const SystemEntry& source = aMaster.at( i );
      SystemPair pair;
      pair.source = source;
      pair.targetExists = found[i] >= 0;
      pair.target = found[i] >= 0 ? candidates.at( found[i] ) : systemAt( aTargetRoot, source.folderName );
      pairs.append( pair );
   }
   return pairs;
}

SystemPlan planSystem( const SystemPair& aPair, const SyncProgress& aProgress )
{
   SystemPlan plan;
   plan.pair = aPair;

   const auto proceed = [&]( const QString& aMessage ) {
      if ( aProgress && !aProgress( aMessage ) )
         plan.canceled = true;
      return !plan.canceled;
   };

   const QString name = aPair.source.folderName;
   if ( sameLocation( aPair.source.gamelistPath, aPair.target.gamelistPath ) ) {
      plan.problems << QStringLiteral( "%1: the target is the master's own gamelist." ).arg( name );
      return plan;
   }

   if ( !proceed( QStringLiteral( "Reading the master's %1 gamelist" ).arg( name ) ) )
      return plan;
   Gamelist master;
   if ( const Result<> loaded = master.load( aPair.source, true ); !loaded ) {
      plan.problems << loaded.error();
      return plan;
   }

   Gamelist target;  // stays empty when the target has no gamelist yet
   if ( aPair.targetExists ) {
      if ( !proceed( QStringLiteral( "Reading the target's %1 gamelist" ).arg( aPair.target.folderName ) ) )
         return plan;
      if ( const Result<> loaded = target.load( aPair.target, true ); !loaded ) {
         plan.problems << loaded.error();
         return plan;
      }
   }

   const bool foldsCase = target.foldsCase();
   plan.copyRoms = !sameLocation( master.romDir(), romDirOf( aPair.target ) );
   const QHash<QString, int> targetKeys = romIndex( target, foldsCase );

   QSet<int> matched;
   QSet<QString> seen;
   for ( int i = 0; i < master.count(); ++i ) {
      const Game& source = master.at( i );
      if ( !proceed( QStringLiteral( "%1: comparing game %2 of %3, %4" )
                        .arg( name )
                        .arg( i + 1 )
                        .arg( master.count() )
                        .arg( source.name ) ) )
         return plan;

      const QString key = romKey( master, source.physicalRomPath, foldsCase );
      const int targetIndex = targetKeys.value( key, -1 );
      if ( const QString problem = rejection( source, seen.contains( key ), targetIndex >= 0 ); !problem.isEmpty() ) {
         plan.problems << problem;
         continue;
      }
      seen.insert( key );
      if ( targetIndex >= 0 )
         matched.insert( targetIndex );

      const SyncGame game = planGame( plan, master, target, i, targetIndex );
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

SyncPlan planSync( const QVector<SystemEntry>& aMaster, const QString& aTargetRoot, const PlanProgress& aProgress )
{
   SyncPlan plan;
   const QVector<SystemPair> pairs = pairSystems( aMaster, aTargetRoot );
   const int total = static_cast<int>( pairs.size() );

   for ( int done = 0; done < total; ++done ) {
      const SyncProgress relay = [&]( const QString& aMessage ) {
         return !aProgress || aProgress( aMessage, done, total );
      };
      SystemPlan system = planSystem( pairs.at( done ), relay );
      if ( system.canceled ) {
         plan.systems.clear();
         plan.canceled = true;
         break;
      }
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
      report.error = report.canceled ? QStringLiteral( "Canceled." ) : aMessage;
      return report;
   };
   const Proceed proceed = [&]( const QString& aMessage ) {
      if ( aProgress && !aProgress( aMessage ) )
         report.canceled = true;
      return !report.canceled;
   };

   Gamelist master;
   if ( const Result<> loaded = master.load( aPlan.pair.source, true ); !loaded )
      return fail( loaded.error() );
   if ( const Result<> unchanged = checkMasterUnchanged( aPlan, master ); !unchanged )
      return fail( unchanged.error() );

   // Before the ROMs too, so a stale plan copies nothing.
   Gamelist target;
   if ( !aPlan.createTarget ) {
      if ( const Result<> ready = loadTarget( aPlan, master, target ); !ready )
         return fail( ready.error() );
   }

   if ( const Result<> copied = copyPlannedRoms( aPlan, proceed ); !copied )
      return fail( copied.error() );

   if ( !proceed( QStringLiteral( "%1: writing the gamelist and media" ).arg( report.name ) ) )
      return fail( QStringLiteral( "Canceled." ) );

   if ( aPlan.createTarget ) {
      if ( const Result<> made = createGamelist( aPlan.pair.target.gamelistPath ); !made )
         return fail( made.error() );
   }

   // Again: the copy may have run long enough for EmulationStation to rewrite the gamelist.
   if ( const Result<> ready = loadTarget( aPlan, master, target ); !ready )
      return fail( ready.error() );

   const Result<QHash<int, QString>> added = addNewGames( aPlan, target, report );
   if ( !added )
      return fail( added.error() );

   const TargetChanges changes = collectChanges( aPlan, master, target, added.value(), report );
   if ( const Result<> written = writeChanges( changes, target, report, proceed ); !written )
      return fail( written.error() );
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

int syncStepCount( const SyncPlan& aPlan )
{
   int steps = 0;
   for ( const SystemPlan& system : aPlan.systems ) {
      if ( system.isEmpty() )
         continue;
      for ( const SyncGame& game : system.games )
         steps += ( system.copyRoms ? static_cast<int>( game.rom.files.size() ) : 0 ) + game.copyImage + game.copyVideo;
      ++steps;
   }
   return steps;
}
