// Gamelist: the root scan, the parse, and the filter predicate. Everything here runs
// against a real gamelist.xml written into a QTemporaryDir, because the three
// filesystem-backed flags are the point.

#include "GamelistFilter.h"
#include "GamelistFixture.h"
#include "MediaOps.h"
#include "QtPrinters.h"

#include <gtest/gtest.h>

#include <filesystem>

namespace {

TEST( ScanSystems, FindsExtensionlessFolders )
{
   Fixture fixture;

   const QVector<SystemEntry> systems = scanSystems( fixture.dir.path() );

   // System folders have no dot in their name, so a "*.*" glob would find none.
   ASSERT_EQ( systems.size(), 2 );
   EXPECT_EQ( systems.at( 0 ).folderName, QStringLiteral( "megadrive" ) );
   EXPECT_EQ( systems.at( 0 ).kind, skMegaDrive );
   EXPECT_EQ( systems.at( 1 ).folderName, QStringLiteral( "snes" ) );
   EXPECT_EQ( systems.at( 1 ).gamelistPath, fixture.path( "snes/gamelist.xml" ) );
}

TEST( ScanSystems, IgnoresAFolderWithNoGamelistAndAnAbsentRoot )
{
   Fixture fixture;

   for ( const SystemEntry& system : scanSystems( fixture.dir.path() ) )
      EXPECT_NE( system.folderName, QStringLiteral( "no-gamelist-here" ) );

   EXPECT_TRUE( scanSystems( QStringLiteral( "/nonexistent" ) ).isEmpty() );
}

TEST( Gamelist, ParsesGamesAndSkipsEmptyNodes )
{
   Fixture fixture;
   Gamelist list;

   const Result<> loaded = list.load( fixture.snes );
   ASSERT_TRUE( loaded ) << loaded.error().toStdString();

   // The <game/> with no children was never a game.
   ASSERT_EQ( list.count(), 3 );

   const Game& sonic = list.at( 0 );
   EXPECT_EQ( sonic.name, QStringLiteral( "Sonic The Hedgehog" ) );
   EXPECT_EQ( sonic.description, QStringLiteral( "Gotta go fast" ) );
   EXPECT_EQ( sonic.romName(), QStringLiteral( "Sonic.zip" ) );
   EXPECT_EQ( sonic.releaseDate, QStringLiteral( "23/06/1991" ) );
   EXPECT_EQ( sonic.favorite, 1 );
   EXPECT_EQ( sonic.kidGame, 0 );

   // A zero month and day mean the year is all that was known.
   EXPECT_EQ( list.at( 1 ).releaseDate, QStringLiteral( "1991" ) );
   // Only the literal "true" is true, so "False" and a missing node agree.
   EXPECT_EQ( list.at( 1 ).hidden, 0 );
   EXPECT_EQ( list.at( 1 ).kidGame, 1 );

   // A missing node reads as empty, not an error.
   EXPECT_TRUE( list.at( 2 ).imagePath.isEmpty() );
   EXPECT_TRUE( list.at( 2 ).physicalImagePath.isEmpty() );
}

TEST( Gamelist, RomDirIsIndependentOfTheGamelistFolder )
{
   Fixture fixture;
   writeFile( fixture.path( "elsewhere/Sonic.zip" ), "rom" );
   writeFile( fixture.path( "elsewhere/media/images/Sonic.png" ), "png" );

   SystemEntry system = fixture.snes;
   system.romDir = fixture.path( "elsewhere" );
   Gamelist list;
   ASSERT_TRUE( list.load( system ) );

   EXPECT_EQ( list.romDir(), fixture.path( "elsewhere" ) );
   EXPECT_EQ( list.at( 0 ).physicalRomPath, fixture.path( "elsewhere/Sonic.zip" ) );
   EXPECT_FALSE( list.at( 0 ).missingImage ) << "media links resolve against romDir";
   EXPECT_TRUE( list.at( 0 ).missingVideo );
}

TEST( Gamelist, NodesStayAlignedWithGames )
{
   Fixture fixture;
   Gamelist list;

   ASSERT_TRUE( list.load( fixture.snes ) );

   for ( int index = 0; index < list.count(); ++index )
      EXPECT_EQ( list.domText( index, "path" ), list.at( index ).romPath );

   EXPECT_TRUE( list.domText( list.count(), "path" ).isEmpty() );
   EXPECT_TRUE( list.domText( -1, "path" ).isEmpty() );
}

// The ROM and the picture are on disk under a different case than the gamelist
// spells them. That has to resolve on a case-sensitive filesystem, or every such game
// is an orphan - and "delete orphans from gamelist" would then offer to delete the
// user's library.
TEST( Gamelist, ResolvesPathsWhoseCaseDoesNotMatchTheDisk )
{
   SKIP_ON_CASE_INSENSITIVE_FS();
   Fixture fixture;
   Gamelist list;

   ASSERT_TRUE( list.load( fixture.snes ) );

   EXPECT_EQ( list.at( 0 ).physicalImagePath, fixture.path( "snes/media/images/sonic.PNG" ) );
   EXPECT_FALSE( list.at( 0 ).missingImage );
   EXPECT_FALSE( list.at( 0 ).isOrphan );

   EXPECT_EQ( list.at( 1 ).physicalRomPath, fixture.path( "snes/sub/SONIC.ZIP" ) );
   EXPECT_FALSE( list.at( 1 ).isOrphan );

   // A file that is really absent stays absent, and keeps the path it was given.
   EXPECT_TRUE( list.at( 1 ).missingImage );
   EXPECT_EQ( list.at( 1 ).physicalImagePath, fixture.path( "snes/media/images/absent.png" ) );
   EXPECT_TRUE( list.at( 2 ).isOrphan );

   // An empty <video> counts as missing, exactly like one that points nowhere.
   EXPECT_FALSE( list.at( 0 ).missingVideo );
   EXPECT_TRUE( list.at( 2 ).missingVideo );
}

// An unreadable, malformed or gameless gamelist is reported, not silently dropped.
TEST( Gamelist, ReportsWhatItCannotLoad )
{
   Fixture fixture;
   Gamelist list;

   const Result<> absent =
      list.load( { QStringLiteral( "x" ), skOther, fixture.path( "snes/absent.xml" ) } );
   EXPECT_FALSE( absent );
   EXPECT_FALSE( absent.error().isEmpty() );

   writeFile( fixture.path( "broken/gamelist.xml" ), "<gameList><game>" );
   const Result<> broken =
      list.load( { QStringLiteral( "broken" ), skOther, fixture.path( "broken/gamelist.xml" ) } );
   EXPECT_FALSE( broken );
   EXPECT_TRUE( broken.error().contains( QStringLiteral( "not valid XML" ) ) )
      << broken.error().toStdString();

   writeFile( fixture.path( "empty/gamelist.xml" ), "<gameList/>" );
   const Result<> empty =
      list.load( { QStringLiteral( "empty" ), skOther, fixture.path( "empty/gamelist.xml" ) } );
   EXPECT_FALSE( empty );
   EXPECT_TRUE( empty.error().contains( QStringLiteral( "no game" ) ) )
      << empty.error().toStdString();
}

TEST( Gamelist, FiltersOnTheGameItself )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   const auto matches = [&list]( FilterCategory aCategory ) {
      QVector<int> kept;
      FilterSpec filter;
      filter.category = aCategory;
      for ( int index = 0; index < list.count(); ++index )
         if ( filterAccepts( list, index, filter ) )
            kept.append( index );
      return kept;
   };

   EXPECT_EQ( matches( FilterCategory::All ), QVector<int>( { 0, 1, 2 } ) );
   EXPECT_EQ( matches( FilterCategory::MissingImage ), QVector<int>( { 1, 2 } ) );
   EXPECT_EQ( matches( FilterCategory::MissingVideo ), QVector<int>( { 1, 2 } ) );
   EXPECT_EQ( matches( FilterCategory::MissingDate ), QVector<int>( { 2 } ) );
   EXPECT_EQ( matches( FilterCategory::MissingPublisher ), QVector<int>( { 1 } ) );
   EXPECT_EQ( matches( FilterCategory::KidGame ), QVector<int>( { 1 } ) );
   EXPECT_EQ( matches( FilterCategory::Favorite ), QVector<int>( { 0 } ) );
   EXPECT_EQ( matches( FilterCategory::Orphan ), QVector<int>( { 2 } ) );
   EXPECT_EQ( matches( FilterCategory::DuplicateNames ), QVector<int>( { 0, 1 } ) );
   EXPECT_EQ( matches( FilterCategory::DuplicateRoms ), QVector<int>( { 0, 1 } ) );
}

// The combo is built from this order, and filterDependsOnSelection is a range check on
// it: a category added in the wrong place would silently change what an item filters.
TEST( FilterCategories, OnlyTheSameAsCategoriesDependOnTheSelection )
{
   for ( int value = 0; value < static_cast<int>( FilterCategory::Count ); ++value ) {
      const FilterCategory category = static_cast<FilterCategory>( value );
      const bool sameAs = category >= FilterCategory::SameRegion &&
                          category <= FilterCategory::SameRom;
      EXPECT_EQ( filterDependsOnSelection( category ), sameAs ) << value;
   }

   // The numbering is fixed: anything that stored a combo index would rely on it.
   EXPECT_EQ( static_cast<int>( FilterCategory::Orphan ), 14 );
   EXPECT_EQ( static_cast<int>( FilterCategory::SameRegion ), 15 );
   EXPECT_EQ( static_cast<int>( FilterCategory::SameRom ), 28 );
   EXPECT_EQ( static_cast<int>( FilterCategory::DuplicateRoms ), 30 );
   EXPECT_EQ( static_cast<int>( FilterCategory::Count ), 31 );
}

TEST( Gamelist, SameAsSelectedNeedsASelection )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   FilterSpec filter;
   filter.category = FilterCategory::SamePublisher;

   // No reference game, so nothing matches.
   EXPECT_FALSE( filterAccepts( list, 0, filter ) );

   filter.reference = 0;
   EXPECT_TRUE( filterAccepts( list, 2, filter ) );  // Sega, like the reference
   EXPECT_FALSE( filterAccepts( list, 1, filter ) );

   // Ratings compare numerically, and "0,8" is the same number as "0.8".
   filter.category = FilterCategory::SameOrLowerRating;
   EXPECT_TRUE( filterAccepts( list, 1, filter ) );
   EXPECT_FALSE( filterAccepts( list, 2, filter ) );  // no rating at all, so never
   filter.category = FilterCategory::SameOrHigherRating;
   EXPECT_FALSE( filterAccepts( list, 1, filter ) );

   // Release date <= reference, over the shortened display forms: "1991" is the
   // 1st of January, which is before the reference's 23rd of June.
   filter.category = FilterCategory::SameOrEarlierDate;
   EXPECT_TRUE( filterAccepts( list, 1, filter ) );
   filter.category = FilterCategory::SameOrLaterDate;
   EXPECT_FALSE( filterAccepts( list, 1, filter ) );

   // Same folder, and same ROM file name across different folders.
   filter.category = FilterCategory::SameFolder;
   EXPECT_TRUE( filterAccepts( list, 2, filter ) );
   EXPECT_FALSE( filterAccepts( list, 1, filter ) );
   filter.category = FilterCategory::SameRom;
   EXPECT_TRUE( filterAccepts( list, 1, filter ) );
}

TEST( Gamelist, SearchMatchesWhateverTheListShows )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   FilterSpec filter;
   EXPECT_EQ( displayName( list, 0, filter ), QStringLiteral( "Sonic The Hedgehog" ) );

   filter.listByRom = true;
   EXPECT_EQ( displayName( list, 1, filter ), QStringLiteral( "Sonic.zip" ) );

   filter.fullRomName = true;
   EXPECT_EQ( displayName( list, 1, filter ), QStringLiteral( "sub/Sonic.zip" ) );

   // The search is case-insensitive, and only the rom path holds "sub".
   filter.search = QStringLiteral( "SUB/" );
   EXPECT_TRUE( filterAccepts( list, 1, filter ) );
   EXPECT_FALSE( filterAccepts( list, 0, filter ) );

   filter.listByRom = false;
   EXPECT_FALSE( filterAccepts( list, 1, filter ) );

   // The search narrows the category; it does not widen it.
   filter.category = FilterCategory::Orphan;
   filter.search = QStringLiteral( "sonic" );
   EXPECT_FALSE( filterAccepts( list, 0, filter ) );
}

TEST( Gamelist, ConvertsReleaseDatesBothWays )
{
   EXPECT_EQ( gamelistDateToDisplay( QStringLiteral( "19910623T000000" ) ),
              QStringLiteral( "23/06/1991" ) );
   EXPECT_EQ( gamelistDateToDisplay( QStringLiteral( "19910600T000000" ) ),
              QStringLiteral( "06/1991" ) );
   EXPECT_EQ( gamelistDateToDisplay( QStringLiteral( "19910000T000000" ) ),
              QStringLiteral( "1991" ) );
   EXPECT_TRUE( gamelistDateToDisplay( QString() ).isEmpty() );
   EXPECT_TRUE( gamelistDateToDisplay( QStringLiteral( "not a date" ) ).isEmpty() );

   EXPECT_EQ( displayDateToGamelist( QStringLiteral( "23/06/1991" ) ),
              QStringLiteral( "19910623T000000" ) );
   EXPECT_EQ( displayDateToGamelist( QStringLiteral( "06/1991" ) ),
              QStringLiteral( "19910600T000000" ) );
   EXPECT_EQ( displayDateToGamelist( QStringLiteral( "1991" ) ),
              QStringLiteral( "19910000T000000" ) );
   EXPECT_TRUE( displayDateToGamelist( QStringLiteral( "june 1991" ) ).isEmpty() );
   EXPECT_TRUE( displayDateToGamelist( QString() ).isEmpty() );

   // What load() stores round-trips through what the save path will write.
   for ( const QString& stored :
         { QStringLiteral( "19910623T000000" ), QStringLiteral( "19910600T000000" ),
           QStringLiteral( "19910000T000000" ) } ) {
      EXPECT_EQ( displayDateToGamelist( gamelistDateToDisplay( stored ) ), stored );
   }
}

// --- write ------------------------------------------------------

TEST( Gamelist, SetFieldsWritesOnlyTheFieldsItIsGiven )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   GameFields values = gameFields( list.at( 0 ) );
   values.genre = QStringLiteral( "Platformer" );
   values.favorite = 0;

   const GameFieldSet which = changedFields( list.at( 0 ), values );
   EXPECT_EQ( which, GameFieldSet( gfGenre | gfFavorite ) );

   const Result<> written = list.setFields( { 0 }, values, which );
   ASSERT_TRUE( written ) << written.error().toStdString();

   const Gamelist saved = reload( fixture );
   EXPECT_EQ( saved.at( 0 ).genre, QStringLiteral( "Platformer" ) );
   EXPECT_EQ( saved.at( 0 ).favorite, 0 );
   // Everything else survived the round trip untouched, including the game the
   // save was not about.
   EXPECT_EQ( saved.at( 0 ).description, QStringLiteral( "Gotta go fast" ) );
   EXPECT_EQ( saved.at( 0 ).rating, QStringLiteral( "0,8" ) );
   EXPECT_EQ( saved.at( 1 ).kidGame, 1 );
   EXPECT_EQ( saved.count(), 3 );
}

// A node that does not exist yet is created (an absent <image>, say, must not be
// an error).
TEST( Gamelist, SetFieldsCreatesMissingNodes )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   GameFields values;
   values.genre = QStringLiteral( "Shooter" );
   values.hidden = 1;

   ASSERT_TRUE( list.at( 2 ).genre.isEmpty() );
   ASSERT_TRUE( list.setFields( { 2 }, values, filledFields( values ) ) );

   const Gamelist saved = reload( fixture );
   EXPECT_EQ( saved.at( 2 ).genre, QStringLiteral( "Shooter" ) );
   EXPECT_EQ( saved.at( 2 ).hidden, 1 );
   // Written as the literal "true", which is the only thing ES reads as true.
   EXPECT_EQ( saved.domText( 2, "hidden" ), QStringLiteral( "true" ) );
}

TEST( Gamelist, BatchSetFieldsLeavesTheBlanksAlone )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   GameFields values;
   values.publisher = QStringLiteral( "Sega of America" );
   values.kidGame = 0;

   ASSERT_TRUE( list.setFields( { 0, 1, 2 }, values, filledFields( values ) ) );

   const Gamelist saved = reload( fixture );
   for ( int index = 0; index < saved.count(); ++index ) {
      EXPECT_EQ( saved.at( index ).publisher, QStringLiteral( "Sega of America" ) );
      EXPECT_EQ( saved.at( index ).kidGame, 0 );
   }

   // The names were not filled in, so no game was renamed to the empty string.
   EXPECT_EQ( saved.at( 0 ).name, QStringLiteral( "Sonic The Hedgehog" ) );
   EXPECT_EQ( saved.at( 2 ).name, QStringLiteral( "Vanished" ) );
   EXPECT_EQ( saved.at( 0 ).description, QStringLiteral( "Gotta go fast" ) );
}

// The hint under Edt_ReleaseDate promises that anything outside the three
// accepted forms is saved as blank.
TEST( Gamelist, AnUnparseableDateIsStoredBlank )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   GameFields values = gameFields( list.at( 0 ) );
   values.releaseDate = QStringLiteral( "june 1991" );
   ASSERT_TRUE( list.setFields( { 0 }, values, changedFields( list.at( 0 ), values ) ) );

   EXPECT_TRUE( list.at( 0 ).releaseDate.isEmpty() );
   EXPECT_TRUE( reload( fixture ).at( 0 ).releaseDate.isEmpty() );

   // And a good one still converts on the way out.
   values.releaseDate = QStringLiteral( "06/1991" );
   ASSERT_TRUE( list.setFields( { 0 }, values, GameFieldSet( gfReleaseDate ) ) );
   EXPECT_EQ( reload( fixture ).at( 0 ).releaseDate, QStringLiteral( "06/1991" ) );
}

TEST( Gamelist, SetImageReEncodesIntoTheSystemsImageFolder )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   // Taken from the first game that has one, not guessed.
   EXPECT_EQ( list.imageFolder(), QStringLiteral( "./media/images/" ) );

   // Out of the .qrc, which is the reason this is a load-and-save and not a copy.
   const Result<> set = list.setImage( 2, QStringLiteral( ":/DefaultPictures/default.png" ) );
   ASSERT_TRUE( set ) << set.error().toStdString();

   EXPECT_EQ( list.at( 2 ).imagePath, QStringLiteral( "./media/images/Vanished.png" ) );
   EXPECT_FALSE( list.at( 2 ).missingImage );
   EXPECT_TRUE( QFileInfo::exists( fixture.path( "snes/media/images/Vanished.png" ) ) );

   // setImage saves by itself: it has already written to the disk, and the two
   // must not be left disagreeing.
   EXPECT_EQ( reload( fixture ).at( 2 ).imagePath,
              QStringLiteral( "./media/images/Vanished.png" ) );
}

TEST( Gamelist, ChangeAllOnlyTouchesGamesWithNoPicture )
{
   SKIP_ON_CASE_INSENSITIVE_FS();
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   const QString kept = list.at( 0 ).physicalImagePath;

   // Two of the three have no picture; the third one's art must not be overwritten.
   const Result<int> changed =
      list.setDefaultImageForMissing( QStringLiteral( ":/DefaultPictures/default.png" ) );
   ASSERT_TRUE( changed ) << changed.error().toStdString();
   EXPECT_EQ( changed.value(), 2 );

   EXPECT_EQ( list.at( 0 ).physicalImagePath, kept );
   EXPECT_EQ( QFile( kept ).size(), 3 );  // still the fixture's three bytes
   EXPECT_FALSE( list.at( 1 ).missingImage );
   EXPECT_FALSE( list.at( 2 ).missingImage );
}

TEST( Gamelist, RemoveImageKeepsAFileTwoGamesShare )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   // Game 1 would get its own file name, so link it by hand.
   ASSERT_TRUE( list.setImage( 0, QStringLiteral( ":/DefaultPictures/default.png" ) ) );
   ASSERT_TRUE( list.linkMedia( { { 1, true, list.at( 0 ).physicalImagePath } } ) );
   ASSERT_EQ( list.at( 1 ).imagePath, list.at( 0 ).imagePath );

   const QString shared = list.at( 1 ).physicalImagePath;
   ASSERT_EQ( shared, list.at( 0 ).physicalImagePath );
   ASSERT_TRUE( list.removeImage( 1 ) );

   EXPECT_TRUE( list.at( 1 ).imagePath.isEmpty() );
   EXPECT_TRUE( list.at( 1 ).missingImage );
   EXPECT_TRUE( QFileInfo::exists( shared ) ) << "the other game still links to it";

   // Now nothing else does, so the file goes with the link.
   ASSERT_TRUE( list.removeImage( 0 ) );
   EXPECT_FALSE( QFileInfo::exists( shared ) );
   EXPECT_TRUE( reload( fixture ).at( 0 ).imagePath.isEmpty() );
}

TEST( Gamelist, RemoveImageKeepsAFileTwoGamesShareUnderDifferentCase )
{
   SKIP_ON_CASE_INSENSITIVE_FS();
   Fixture fixture;
   // Two spellings of the one snes/media/images/sonic.PNG the fixture puts on disk.
   writeFile( fixture.path( "snes/gamelist.xml" ), R"(<gameList>
   <game><path>./a.zip</path><image>./media/images/Sonic.png</image></game>
   <game><path>./b.zip</path><image>./media/images/sonic.png</image></game>
</gameList>)" );

   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   ASSERT_EQ( list.at( 0 ).physicalImagePath, list.at( 1 ).physicalImagePath );
   const QString shared = list.at( 0 ).physicalImagePath;
   ASSERT_TRUE( QFileInfo::exists( shared ) );

   ASSERT_TRUE( list.removeImage( 0 ) );
   EXPECT_TRUE( QFileInfo::exists( shared ) ) << "game 1 still links to it";

   ASSERT_TRUE( list.removeImage( 1 ) );
   EXPECT_FALSE( QFileInfo::exists( shared ) );
}

// Windows may refuse without privileges.
bool makeDirSymlink( const QString& aTarget, const QString& aLink )
{
   std::error_code error;
   std::filesystem::create_directory_symlink( aTarget.toStdWString(), aLink.toStdWString(), error );
   return !error;
}

TEST( Gamelist, SharedFileGuardsSeeThroughASymlinkedFolder )
{
   Fixture fixture;
   writeFile( fixture.path( "snes/real/Sonic.png" ), "png" );
   writeFile( fixture.path( "snes/real/Sonic.zip" ), "rom" );
   if ( !makeDirSymlink( fixture.path( "snes/real" ), fixture.path( "snes/alias" ) ) )
      GTEST_SKIP() << "cannot create symlinks";
   writeFile( fixture.path( "snes/gamelist.xml" ), R"(<gameList>
   <game><path>./real/Sonic.zip</path><image>./real/Sonic.png</image></game>
   <game><path>./alias/Sonic.zip</path><image>./alias/Sonic.png</image></game>
</gameList>)" );

   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   EXPECT_EQ( duplicateIndexes( list ), QVector<int>( { 1 } ) );

   ASSERT_TRUE( list.removeImage( 0 ) );
   EXPECT_TRUE( QFileInfo::exists( fixture.path( "snes/real/Sonic.png" ) ) ) << "game 1 still links to it";

   ASSERT_TRUE( list.removeGame( 0 ) );
   EXPECT_TRUE( QFileInfo::exists( fixture.path( "snes/real/Sonic.zip" ) ) ) << "game 0 was the twin";

   ASSERT_TRUE( list.removeImage( 0 ) );
   EXPECT_FALSE( QFileInfo::exists( fixture.path( "snes/real/Sonic.png" ) ) );
}

TEST( Gamelist, SharedFileGuardsSeeThroughCaseOnAFoldingVolume )
{
   if ( !filesystemIsCaseInsensitive() )
      GTEST_SKIP() << "filesystem keeps case";
   Fixture fixture;
   writeFile( fixture.path( "snes/gamelist.xml" ), R"(<gameList>
   <game><path>./Sonic.zip</path><image>./media/images/Sonic.png</image></game>
   <game><path>./sonic.zip</path><image>./media/images/sonic.png</image></game>
</gameList>)" );

   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   EXPECT_EQ( duplicateIndexes( list ), QVector<int>( { 1 } ) );

   const QString image = list.at( 0 ).physicalImagePath;
   ASSERT_TRUE( list.removeImage( 0 ) );
   EXPECT_TRUE( QFileInfo::exists( image ) ) << "game 1 still links to it";

   ASSERT_TRUE( list.removeGame( 0 ) );
   EXPECT_TRUE( QFileInfo::exists( fixture.path( "snes/Sonic.zip" ) ) ) << "game 0 was the twin";
}

TEST( Gamelist, RemoveGameDropsTheEntryAndItsFiles )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   const Result<QStringList> removed = list.removeGame( 0 );
   ASSERT_TRUE( removed ) << removed.error().toStdString();
   EXPECT_TRUE( removed.value().isEmpty() ) << "every file could be deleted";

   ASSERT_EQ( list.count(), 2 );
   EXPECT_EQ( list.at( 0 ).romPath, QStringLiteral( "./sub/Sonic.zip" ) );
   EXPECT_FALSE( QFileInfo::exists( fixture.path( "snes/Sonic.zip" ) ) );
   EXPECT_FALSE( QFileInfo::exists( fixture.path( "snes/media/images/sonic.PNG" ) ) );
   EXPECT_FALSE( QFileInfo::exists( fixture.path( "snes/media/videos/Sonic.mp4" ) ) );
   EXPECT_TRUE( QFileInfo::exists( fixture.path( "snes/sub/SONIC.ZIP" ) ) );

   // Written through, and the node list still lines up with the games.
   EXPECT_EQ( reload( fixture ).count(), 2 );
   EXPECT_EQ( list.domText( 0, "path" ), list.at( 0 ).romPath );

   EXPECT_FALSE( list.removeGame( 2 ) );
}

TEST( Gamelist, RemoveGameCanKeepTheFiles )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   const Result<QStringList> removed = list.removeGame( 0, false );
   ASSERT_TRUE( removed ) << removed.error().toStdString();
   EXPECT_TRUE( removed.value().isEmpty() );

   EXPECT_EQ( list.count(), 2 );
   EXPECT_EQ( reload( fixture ).count(), 2 );
   EXPECT_TRUE( QFileInfo::exists( fixture.path( "snes/Sonic.zip" ) ) );
   EXPECT_TRUE( QFileInfo::exists( fixture.path( "snes/media/images/sonic.PNG" ) ) );
   EXPECT_TRUE( QFileInfo::exists( fixture.path( "snes/media/videos/Sonic.mp4" ) ) );
}

TEST( Gamelist, RemoveGamesDropsOnlyTheEntries )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   // Game 2 (Vanished.zip) is the one orphan; Sonic.zip in two folders is two games.
   EXPECT_EQ( orphanIndexes( list ), QVector<int>( { 2 } ) );
   EXPECT_TRUE( duplicateIndexes( list ).isEmpty() );

   const Result<> dropped = list.removeGames( { 0, 2, 99 } );
   ASSERT_TRUE( dropped ) << dropped.error().toStdString();

   ASSERT_EQ( list.count(), 1 );
   EXPECT_EQ( list.at( 0 ).romPath, QStringLiteral( "./sub/Sonic.zip" ) );
   EXPECT_EQ( list.domText( 0, "path" ), list.at( 0 ).romPath );
   EXPECT_EQ( reload( fixture ).count(), 1 );
   // Unlike removeGame, no file goes with the entry.
   EXPECT_TRUE( QFileInfo::exists( fixture.path( "snes/Sonic.zip" ) ) );
   EXPECT_TRUE( QFileInfo::exists( fixture.path( "snes/media/videos/Sonic.mp4" ) ) );
}

TEST( Gamelist, DuplicateIndexesKeepTheFirstOfEachRom )
{
   Fixture fixture;
   writeFile( fixture.path( "snes/gamelist.xml" ), R"(<gameList>
   <game><path>./Sonic.zip</path><name>A</name></game>
   <game><path>./sub/Sonic.zip</path><name>B</name></game>
   <game><path>./Sonic.zip</path><name>C</name></game>
   <game><path>./Sonic.zip</path><name>D</name></game>
</gameList>)" );
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   EXPECT_EQ( duplicateIndexes( list ), QVector<int>( { 2, 3 } ) );

   ASSERT_TRUE( list.removeGames( duplicateIndexes( list ) ) );
   EXPECT_EQ( list.count(), 2 );
   EXPECT_EQ( list.at( 0 ).name, QStringLiteral( "A" ) );
   EXPECT_EQ( list.at( 1 ).name, QStringLiteral( "B" ) );
   EXPECT_TRUE( duplicateIndexes( list ).isEmpty() );
}

TEST( Gamelist, SetAndRemoveVideo )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   writeFile( fixture.path( "elsewhere/clip.mp4" ), "video" );
   Result<> set = list.setVideo( 2, fixture.path( "elsewhere/clip.mp4" ) );
   ASSERT_TRUE( set ) << set.error().toStdString();

   EXPECT_EQ( list.at( 2 ).videoPath, QStringLiteral( "./media/videos/Vanished.mp4" ) );
   EXPECT_TRUE( QFileInfo::exists( fixture.path( "snes/media/videos/Vanished.mp4" ) ) );

   // Choosing the game's own video must not be remove-then-copy onto itself.
   set = list.setVideo( 2, list.at( 2 ).physicalVideoPath );
   ASSERT_TRUE( set ) << set.error().toStdString();
   EXPECT_EQ( QFile( list.at( 2 ).physicalVideoPath ).size(), 5 );

   ASSERT_TRUE( list.removeVideo( 2 ) );
   EXPECT_FALSE( QFileInfo::exists( fixture.path( "snes/media/videos/Vanished.mp4" ) ) );
   EXPECT_TRUE( reload( fixture ).at( 2 ).videoPath.isEmpty() );
}

TEST( Gamelist, WritesFollowAnExistingFolderUnderDifferentCase )
{
   SKIP_ON_CASE_INSENSITIVE_FS();
   Fixture fixture;
   // The gamelist says ./media/..., the disk says Media.
   ASSERT_TRUE( QDir().rename( fixture.path( "snes/media" ), fixture.path( "snes/Media" ) ) );

   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   writeFile( fixture.path( "elsewhere/clip.mp4" ), "video" );
   const Result<> video = list.setVideo( 2, fixture.path( "elsewhere/clip.mp4" ) );
   ASSERT_TRUE( video ) << video.error().toStdString();
   EXPECT_EQ( list.at( 2 ).physicalVideoPath, fixture.path( "snes/Media/videos/Vanished.mp4" ) );
   EXPECT_TRUE( QFileInfo::exists( list.at( 2 ).physicalVideoPath ) );

   QImage picture( 2, 2, QImage::Format_RGB32 );
   picture.fill( Qt::red );
   const Result<> image = list.setImage( 2, picture );
   ASSERT_TRUE( image ) << image.error().toStdString();
   EXPECT_EQ( list.at( 2 ).physicalImagePath, fixture.path( "snes/Media/images/Vanished.png" ) );

   EXPECT_FALSE( QFileInfo::exists( fixture.path( "snes/media" ) ) ) << "no second folder";
}

TEST( Gamelist, WritesFollowAPartlyExistingFolderUnderDifferentCase )
{
   SKIP_ON_CASE_INSENSITIVE_FS();
   Fixture fixture;
   // Media/images is there, Media/videos is not yet: the first video save.
   ASSERT_TRUE( QDir( fixture.path( "snes/media/videos" ) ).removeRecursively() );
   ASSERT_TRUE( QDir().rename( fixture.path( "snes/media" ), fixture.path( "snes/Media" ) ) );

   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   writeFile( fixture.path( "elsewhere/clip.mp4" ), "video" );
   const Result<> video = list.setVideo( 2, fixture.path( "elsewhere/clip.mp4" ) );
   ASSERT_TRUE( video ) << video.error().toStdString();
   EXPECT_EQ( list.at( 2 ).physicalVideoPath, fixture.path( "snes/Media/videos/Vanished.mp4" ) );
   EXPECT_TRUE( QFileInfo::exists( list.at( 2 ).physicalVideoPath ) );
   EXPECT_FALSE( QFileInfo::exists( fixture.path( "snes/media" ) ) ) << "no second folder";
}

TEST( Gamelist, HashesAreComputedOnDemandAndAnOrphanHashesToNothing )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   list.ensureHashes( 0 );
   EXPECT_EQ( list.at( 0 ).crc32, fileCrc32( fixture.path( "snes/Sonic.zip" ) ) );
   EXPECT_EQ( list.at( 0 ).md5.size(), 32 );
   EXPECT_EQ( list.at( 0 ).sha1.size(), 40 );

   // A ROM that has vanished hashes to an empty string rather than failing.
   list.ensureHashes( 2 );
   EXPECT_TRUE( list.at( 2 ).crc32.isEmpty() );
}

TEST( Gamelist, UnlistedRomsAreTheFilesNoGameCoversAndNoMedia )
{
   Fixture fixture;
   writeFile( fixture.path( "snes/Streets.zip" ), "rom" );
   writeFile( fixture.path( "snes/sub/Zelda.sfc" ), "rom" );
   writeFile( fixture.path( "snes/notes.txt" ), "not a rom" );
   writeFile( fixture.path( "snes/media/images/extra.dat" ), "in the media folder" );

   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   // Sonic.zip and sub/SONIC.ZIP are listed, the second under another case.
   EXPECT_EQ( unlistedRoms( list ), QStringList( { fixture.path( "snes/Streets.zip" ),
                                                  fixture.path( "snes/sub/Zelda.sfc" ) } ) );
}

TEST( Gamelist, AddGamesWritesPathAndNameAndSurvivesAReload )
{
   Fixture fixture;
   writeFile( fixture.path( "snes/Streets.zip" ), "rom" );
   writeFile( fixture.path( "snes/sub/Zelda.sfc" ), "rom" );

   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   // The listed Sonic.zip is skipped, not an error.
   const Result<int> added =
      list.addGames( { fixture.path( "snes/Streets.zip" ), fixture.path( "snes/Sonic.zip" ),
                       fixture.path( "snes/sub/Zelda.sfc" ) } );
   ASSERT_TRUE( added ) << added.error().toStdString();
   ASSERT_EQ( added.value(), 2 );

   ASSERT_EQ( list.count(), 5 );
   EXPECT_EQ( list.at( 3 ).romPath, QStringLiteral( "./Streets.zip" ) );
   EXPECT_EQ( list.at( 3 ).name, QStringLiteral( "Streets" ) );
   EXPECT_FALSE( list.at( 3 ).isOrphan );
   EXPECT_EQ( list.at( 4 ).romPath, QStringLiteral( "./sub/Zelda.sfc" ) );
   EXPECT_TRUE( unlistedRoms( list ).isEmpty() );

   const Gamelist reloaded = reload( fixture );
   ASSERT_EQ( reloaded.count(), 5 );
   EXPECT_EQ( reloaded.at( 4 ).name, QStringLiteral( "Zelda" ) );
}

TEST( Gamelist, AddGamesRefusesAFileOutsideTheSystemAndLeavesTheListAlone )
{
   Fixture fixture;
   writeFile( fixture.path( "snes/Streets.zip" ), "rom" );
   writeFile( fixture.path( "megadrive/Other.zip" ), "rom" );

   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   const Result<int> refused = list.addGames(
      { fixture.path( "snes/Streets.zip" ), fixture.path( "megadrive/Other.zip" ) } );
   EXPECT_FALSE( refused );
   EXPECT_FALSE( refused.error().isEmpty() );
   EXPECT_EQ( list.count(), 3 );
   EXPECT_EQ( reload( fixture ).count(), 3 );

   // And the rolled-back node is really gone from the document: a good add after it
   // must not write the refused batch's first file too.
   const Result<int> added = list.addGames( { fixture.path( "snes/Streets.zip" ) } );
   ASSERT_TRUE( added ) << added.error().toStdString();
   EXPECT_EQ( added.value(), 1 );
   EXPECT_EQ( reload( fixture ).count(), 4 );
}

QByteArray readAll( const QString& aPath )
{
   QFile file( aPath );
   return file.open( QIODevice::ReadOnly ) ? file.readAll() : QByteArray( "<unreadable>" );
}


TEST( Gamelist, KeepsEveryTopLevelElementThroughASave )
{
   Fixture fixture;
   writeFile( fixture.path( "mame/gamelist.xml" ), R"(<?xml version="1.0"?>
<alternativeEmulator>
	<label>MAME 2003-Plus</label>
</alternativeEmulator>
<gameList>
	<folder>
		<path>./Dir</path>
		<name>Dir</name>
	</folder>
	<game>
		<path>./a.zip</path>
		<name>A</name>
	</game>
</gameList>
)" );

   Gamelist list;
   const SystemEntry mame{ QStringLiteral( "mame" ), skOther, fixture.path( "mame/gamelist.xml" ) };
   ASSERT_TRUE( list.load( mame ) );
   ASSERT_EQ( list.count(), 1 );
   EXPECT_EQ( list.at( 0 ).name, QStringLiteral( "A" ) );

   writeFile( fixture.path( "mame/b.zip" ), "rom" );
   ASSERT_TRUE( list.addGames( { fixture.path( "mame/b.zip" ) } ) );

   const QByteArray saved = readAll( mame.gamelistPath );
   EXPECT_TRUE( saved.startsWith( "<?xml version=\"1.0\"?>\n<alternativeEmulator>" ) ) << saved.toStdString();
   EXPECT_EQ( saved.count( "<alternativeEmulator>" ), 1 );
   EXPECT_LT( saved.indexOf( "</alternativeEmulator>" ), saved.indexOf( "<gameList>" ) );
   EXPECT_FALSE( saved.contains( "gle-root" ) );
   EXPECT_TRUE( saved.contains( "<folder>" ) );

   Gamelist reloaded;
   ASSERT_TRUE( reloaded.load( mame ) );
   EXPECT_EQ( reloaded.count(), 2 );
}

TEST( Gamelist, ReadsADeclaredEncodingAndWritesUtf8 )
{
   Fixture fixture;
   writeFile( fixture.path( "gb/gamelist.xml" ),
              QByteArray( "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n<gameList><game><path>./a.gb</path>"
                          "<name>K\xE4se</name></game></gameList>\n" ) );

   Gamelist list;
   const SystemEntry gb{ QStringLiteral( "gb" ), skOther, fixture.path( "gb/gamelist.xml" ) };
   ASSERT_TRUE( list.load( gb ) );
   EXPECT_EQ( list.at( 0 ).name, QString::fromUtf8( "K\xC3\xA4se" ) );

   ASSERT_TRUE( list.save() );
   EXPECT_TRUE( readAll( gb.gamelistPath ).contains( "K\xC3\xA4se" ) );

   Gamelist reloaded;
   ASSERT_TRUE( reloaded.load( gb ) );
   EXPECT_EQ( reloaded.at( 0 ).name, QString::fromUtf8( "K\xC3\xA4se" ) ) << "the declaration must match the bytes";
}

TEST( Gamelist, SavingBacksUpTheOriginalOncePerLoad )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   const QByteArray original = readAll( fixture.snes.gamelistPath );

   const auto backups = [&fixture]() {
      return QFileInfo( fixture.snes.gamelistPath )
         .dir()
         .entryList( { QStringLiteral( "gamelist.xml.*.bak" ) }, QDir::Files );
   };

   GameFields values = gameFields( list.at( 0 ) );
   values.genre = QStringLiteral( "First" );
   ASSERT_TRUE( list.setFields( { 0 }, values, GameFieldSet( gfGenre ) ) );

   ASSERT_EQ( backups().size(), 1 );
   EXPECT_EQ( readAll( fixture.path( "snes/" ) + backups().first() ), original );

   values.genre = QStringLiteral( "Second" );
   ASSERT_TRUE( list.setFields( { 0 }, values, GameFieldSet( gfGenre ) ) );
   EXPECT_EQ( backups().size(), 1 ) << "a second save in the same load must not add one";
   EXPECT_EQ( readAll( fixture.path( "snes/" ) + backups().first() ), original );
}

TEST( Gamelist, AFailedSetFieldsLeavesGameAndDocumentAsTheyWere )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   breakSave( fixture, list );

   GameFields values;
   values.name = QStringLiteral( "Renamed" );
   values.genre = QStringLiteral( "Shooter" );
   values.hidden = 1;

   EXPECT_FALSE( list.setFields( { 2 }, values, filledFields( values ) ) );
   EXPECT_EQ( list.at( 2 ).name, QStringLiteral( "Vanished" ) );
   EXPECT_TRUE( list.at( 2 ).genre.isEmpty() );
   EXPECT_EQ( list.at( 2 ).hidden, 0 );
   EXPECT_EQ( list.domText( 2, "name" ), QStringLiteral( "Vanished" ) );

   ASSERT_TRUE( QDir( fixture.snes.gamelistPath ).removeRecursively() );
   writeFile( fixture.snes.gamelistPath, Cst_Gamelist );
   ASSERT_TRUE( list.save() );
   const QByteArray saved = readAll( fixture.snes.gamelistPath );
   EXPECT_EQ( saved.count( "<genre" ), 1 ) << "the undo must remove the <genre> it created, not empty it";
   EXPECT_EQ( saved.count( "<hidden" ), 1 ) << "nor leave a <hidden>false</hidden> behind";
   EXPECT_TRUE( saved.contains( "<name>Vanished</name>" ) );
}

TEST( Gamelist, PerGameSetFieldsWritesDifferentValuesInOneSave )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   ASSERT_TRUE( list.nameIsDuplicated( QStringLiteral( "Sonic The Hedgehog" ) ) );

   GameFields first;
   first.name = QStringLiteral( "Sonic 1" );
   GameFields second;
   second.name = QStringLiteral( "Sonic 2" );
   second.genre = QStringLiteral( "Racing" );

   const Result<> written = list.setFields(
      { { 0, first, GameFieldSet( gfName ) }, { 1, second, GameFieldSet( gfName | gfGenre ) } } );
   ASSERT_TRUE( written ) << written.error().toStdString();

   EXPECT_FALSE( list.nameIsDuplicated( QStringLiteral( "Sonic The Hedgehog" ) ) );
   const Gamelist saved = reload( fixture );
   EXPECT_EQ( saved.at( 0 ).name, QStringLiteral( "Sonic 1" ) );
   EXPECT_EQ( saved.at( 0 ).genre, QStringLiteral( "Platform" ) ) << "not in which: untouched";
   EXPECT_EQ( saved.at( 1 ).name, QStringLiteral( "Sonic 2" ) );
   EXPECT_EQ( saved.at( 1 ).genre, QStringLiteral( "Racing" ) );
}

TEST( Gamelist, AFailedPerGameSetFieldsLeavesEveryGameAsItWas )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   breakSave( fixture, list );

   GameFields one;
   one.name = QStringLiteral( "One" );
   GameFields two;
   two.genre = QStringLiteral( "Two" );

   EXPECT_FALSE( list.setFields(
      { { 0, one, GameFieldSet( gfName ) }, { 2, two, GameFieldSet( gfGenre ) } } ) );
   EXPECT_EQ( list.at( 0 ).name, QStringLiteral( "Sonic The Hedgehog" ) );
   EXPECT_EQ( list.domText( 0, "name" ), QStringLiteral( "Sonic The Hedgehog" ) );
   EXPECT_TRUE( list.at( 2 ).genre.isEmpty() );
   EXPECT_TRUE( list.domText( 2, "genre" ).isEmpty() );
   EXPECT_TRUE( list.nameIsDuplicated( QStringLiteral( "Sonic The Hedgehog" ) ) );
}

TEST( Gamelist, EditNamesRenamesEachTargetFromItsOwnName )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   ASSERT_TRUE( list.nameIsDuplicated( QStringLiteral( "Sonic The Hedgehog" ) ) );

   NameEdit edit;
   edit.addChars = true;
   edit.endString = QStringLiteral( " (USA)" );
   const Result<> renamed = list.editNames( { 0, 2 }, edit );
   ASSERT_TRUE( renamed ) << renamed.error().toStdString();

   const Gamelist saved = reload( fixture );
   EXPECT_EQ( saved.at( 0 ).name, QStringLiteral( "Sonic The Hedgehog (USA)" ) );
   EXPECT_EQ( saved.at( 1 ).name, QStringLiteral( "Sonic The Hedgehog" ) );
   EXPECT_EQ( saved.at( 2 ).name, QStringLiteral( "Vanished (USA)" ) );
   EXPECT_FALSE( list.nameIsDuplicated( QStringLiteral( "Sonic The Hedgehog" ) ) )
      << "the duplicate counts must follow the rename";
}

TEST( Gamelist, EditNamesThatChangeNothingDoNotSave )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   breakSave( fixture, list );

   EXPECT_TRUE( list.editNames( { 0, 1, 2 }, NameEdit() ) ) << "a save here would have failed";
}

TEST( Gamelist, AFailedEditNamesLeavesGamesAndDocumentAsTheyWere )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   breakSave( fixture, list );

   NameEdit edit;
   edit.changeCase = true;
   edit.caseIndex = 1;
   EXPECT_FALSE( list.editNames( { 0, 2 }, edit ) );

   EXPECT_EQ( list.at( 0 ).name, QStringLiteral( "Sonic The Hedgehog" ) );
   EXPECT_EQ( list.domText( 2, "name" ), QStringLiteral( "Vanished" ) );
   EXPECT_TRUE( list.nameIsDuplicated( QStringLiteral( "Sonic The Hedgehog" ) ) );
}

TEST( Gamelist, DeleteOldBackupsKeepsTheNewestAndNothingElse )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   for ( const char* stamp : { "20200101-000000", "20210101-000000", "20220101-000000" } )
      writeFile( fixture.path( "snes/gamelist.xml." ) + QLatin1String( stamp ) + ".bak", "x" );
   writeFile( fixture.path( "snes/gamelist.xml.notes.bak" ), "mine" );

   const QStringList found = gamelistBackups( list );
   ASSERT_EQ( found.size(), 3 );
   EXPECT_TRUE( found.first().contains( QStringLiteral( "2022" ) ) ) << "newest first";

   const Result<int> removed = deleteOldBackups( list, 1 );
   ASSERT_TRUE( removed ) << removed.error().toStdString();
   EXPECT_EQ( removed.value(), 2 );
   EXPECT_EQ( gamelistBackups( list ), QStringList( found.first() ) );
   EXPECT_TRUE( QFileInfo::exists( fixture.path( "snes/gamelist.xml.notes.bak" ) ) );
   EXPECT_TRUE( QFileInfo::exists( fixture.snes.gamelistPath ) );
}

QString linkInDom( const Gamelist& aList, int aIndex, const char* aName )
{
   return aList.domText( aIndex, aName );
}

TEST( Gamelist, SetVideoFailedSaveRestoresFilesDocumentAndGame )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   writeFile( fixture.path( "new.mp4" ), "new" );
   const QString video = fixture.path( "snes/media/videos/Sonic.mp4" );
   const Game before = list.at( 0 );

   breakSave( fixture, list );

   const Result<> failed = list.setVideo( 0, fixture.path( "new.mp4" ) );
   EXPECT_FALSE( failed );
   EXPECT_FALSE( failed.error().isEmpty() );
   EXPECT_EQ( readAll( video ), QByteArray( "mp4" ) );
   EXPECT_FALSE( QFileInfo::exists( video + QStringLiteral( ".gle-new" ) ) );
   EXPECT_FALSE( QFileInfo::exists( video + QStringLiteral( ".gle-bak" ) ) );
   EXPECT_EQ( list.at( 0 ).videoPath, before.videoPath );
   EXPECT_EQ( list.at( 0 ).physicalVideoPath, before.physicalVideoPath );
   EXPECT_EQ( linkInDom( list, 0, "video" ), before.videoPath );
}

TEST( Gamelist, SetVideoThatCannotBeCopiedKeepsTheOldVideo )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   const QString video = fixture.path( "snes/media/videos/Sonic.mp4" );

   EXPECT_FALSE( list.setVideo( 0, fixture.path( "absent.mp4" ) ) );
   EXPECT_EQ( readAll( video ), QByteArray( "mp4" ) );
}

TEST( Gamelist, SetImageFailedSaveRestoresTheOldPicture )
{
   SKIP_ON_CASE_INSENSITIVE_FS();
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   // Game 1 has a dead link; game 0 has a picture on disk under another case.
   const QString linkBefore = list.at( 1 ).imagePath;
   const bool missingBefore = list.at( 1 ).missingImage;

   breakSave( fixture, list );

   EXPECT_FALSE( list.setImage( 1, QStringLiteral( ":/DefaultPictures/default.png" ) ) );
   EXPECT_FALSE( QFileInfo::exists( fixture.path( "snes/media/images/Sonic.png.gle-new" ) ) );
   EXPECT_FALSE( QFileInfo::exists( fixture.path( "snes/media/images/Sonic.png" ) ) );
   EXPECT_EQ( list.at( 1 ).imagePath, linkBefore );
   EXPECT_EQ( list.at( 1 ).missingImage, missingBefore );
   EXPECT_EQ( linkInDom( list, 1, "image" ), linkBefore );
}

TEST( Gamelist, SetImageFailedSaveBringsBackAnOverwrittenPicture )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   // Game 0 links ./media/images/Sonic.png; make that exact file exist with known bytes.
   const QString picture = fixture.path( "snes/media/images/Sonic.png" );
   writeFile( picture, "old" );

   breakSave( fixture, list );

   EXPECT_FALSE( list.setImage( 0, QStringLiteral( ":/DefaultPictures/default.png" ) ) );
   EXPECT_EQ( readAll( picture ), QByteArray( "old" ) );
   EXPECT_FALSE( QFileInfo::exists( picture + QStringLiteral( ".gle-bak" ) ) );
}

TEST( Gamelist, ChangeAllIsAllOrNothing )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   const QString written = fixture.path( "snes/media/images/Vanished.png" );

   breakSave( fixture, list );

   const Result<int> failed =
      list.setDefaultImageForMissing( QStringLiteral( ":/DefaultPictures/default.png" ) );
   EXPECT_FALSE( failed );
   EXPECT_FALSE( failed.error().isEmpty() );
   EXPECT_FALSE( QFileInfo::exists( written ) );
   EXPECT_TRUE( list.at( 1 ).missingImage );
   EXPECT_TRUE( list.at( 2 ).missingImage );
   EXPECT_TRUE( list.at( 2 ).imagePath.isEmpty() );
}

constexpr const char* Cst_SameStemGamelist = R"(<gameList>
   <game><path>./Sonic.zip</path></game>
   <game><path>./Sonic.7z</path></game>
</gameList>)";

TEST( Gamelist, NewMediaNeverTakesAFileAnotherGameHas )
{
   Fixture fixture;
   writeFile( fixture.path( "snes/gamelist.xml" ), Cst_SameStemGamelist );
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   QImage red( 2, 2, QImage::Format_RGB32 );
   red.fill( Qt::red );
   QImage blue( 2, 2, QImage::Format_RGB32 );
   blue.fill( Qt::blue );
   ASSERT_TRUE( list.setImage( 0, red ) );
   ASSERT_TRUE( list.setImage( 1, blue ) );

   EXPECT_EQ( list.at( 0 ).imagePath, QStringLiteral( "./media/images/Sonic.png" ) );
   EXPECT_EQ( list.at( 1 ).imagePath, QStringLiteral( "./media/images/Sonic.7z.png" ) );
   EXPECT_EQ( QImage( list.at( 0 ).physicalImagePath ).pixel( 0, 0 ), red.pixel( 0, 0 ) );
   EXPECT_EQ( QImage( list.at( 1 ).physicalImagePath ).pixel( 0, 0 ), blue.pixel( 0, 0 ) );

   writeFile( fixture.path( "clip1.mp4" ), "one" );
   writeFile( fixture.path( "clip2.mp4" ), "two" );
   ASSERT_TRUE( list.setVideo( 0, fixture.path( "clip1.mp4" ) ) );
   ASSERT_TRUE( list.setVideo( 1, fixture.path( "clip2.mp4" ) ) );

   EXPECT_EQ( list.at( 1 ).videoPath, QStringLiteral( "./media/videos/Sonic.7z.mp4" ) );
   EXPECT_EQ( readAll( list.at( 0 ).physicalVideoPath ), QByteArray( "one" ) );
   EXPECT_EQ( readAll( list.at( 1 ).physicalVideoPath ), QByteArray( "two" ) );

   // Replacing a game's own picture is still a replace, not a new name.
   ASSERT_TRUE( list.setImage( 1, red ) );
   EXPECT_EQ( list.at( 1 ).imagePath, QStringLiteral( "./media/images/Sonic.7z.png" ) );
}

TEST( Gamelist, ChangeAllWritesOneFilePerGameEvenWithTheSameStem )
{
   Fixture fixture;
   writeFile( fixture.path( "snes/gamelist.xml" ), Cst_SameStemGamelist );
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   const Result<int> changed =
      list.setDefaultImageForMissing( QStringLiteral( ":/DefaultPictures/default.png" ) );
   ASSERT_TRUE( changed ) << changed.error().toStdString();
   EXPECT_EQ( changed.value(), 2 );
   EXPECT_NE( list.at( 0 ).physicalImagePath, list.at( 1 ).physicalImagePath );
}

// The second swapIn used to delete the first game's backup.
TEST( Gamelist, ChangeAllRollbackBringsBackAFileTheFirstGameReplaced )
{
   Fixture fixture;
   writeFile( fixture.path( "snes/gamelist.xml" ), Cst_SameStemGamelist );
   const QString stray = fixture.path( "snes/media/images/Sonic.png" );
   writeFile( stray, "old" );
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   breakSave( fixture, list );

   EXPECT_FALSE( list.setDefaultImageForMissing( QStringLiteral( ":/DefaultPictures/default.png" ) ) );
   EXPECT_EQ( readAll( stray ), QByteArray( "old" ) );
   EXPECT_FALSE( QFileInfo::exists( stray + QStringLiteral( ".gle-bak" ) ) );
   EXPECT_FALSE( QFileInfo::exists( fixture.path( "snes/media/images/Sonic.7z.png" ) ) );
}

TEST( Gamelist, ARomThatDiffersOnlyInCaseIsAnotherFile )
{
   SKIP_ON_CASE_INSENSITIVE_FS();
   Fixture fixture;
   writeFile( fixture.path( "snes/sonic.zip" ), "rom" );  // beside the listed Sonic.zip
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   EXPECT_EQ( unlistedRoms( list ), QStringList( { fixture.path( "snes/sonic.zip" ) } ) );

   const Result<int> added = list.addGames( { fixture.path( "snes/sonic.zip" ) } );
   ASSERT_TRUE( added ) << added.error().toStdString();
   EXPECT_EQ( added.value(), 1 );
   EXPECT_TRUE( unlistedRoms( list ).isEmpty() );
}

TEST( Gamelist, RemoveImageAndVideoFailedSaveKeepLinksAndFiles )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   const Game before = list.at( 0 );

   breakSave( fixture, list );

   EXPECT_FALSE( list.removeImage( 0 ) );
   EXPECT_FALSE( list.removeVideo( 0 ) );
   EXPECT_EQ( list.at( 0 ).imagePath, before.imagePath );
   EXPECT_EQ( list.at( 0 ).videoPath, before.videoPath );
   EXPECT_EQ( list.at( 0 ).physicalImagePath, before.physicalImagePath );
   EXPECT_EQ( list.at( 0 ).physicalVideoPath, before.physicalVideoPath );
   EXPECT_FALSE( list.at( 0 ).missingImage );
   EXPECT_FALSE( list.at( 0 ).missingVideo );
   EXPECT_EQ( linkInDom( list, 0, "image" ), before.imagePath );
   EXPECT_EQ( linkInDom( list, 0, "video" ), before.videoPath );
   EXPECT_TRUE( QFileInfo::exists( before.physicalImagePath ) );
   EXPECT_TRUE( QFileInfo::exists( before.physicalVideoPath ) );
}

// ES-DE layout: ROMs under roms/snes, media by the ROM's name under media/snes.
struct EsdeLayout
{
   QTemporaryDir dir;
   SystemEntry system;

   explicit EsdeLayout( const QByteArray& aGames = "<game><path>./Sub/Game.zip</path><name>Game</name>"
                                                   "<image>./images/legacy.png</image></game>"
                                                   "<game><path>./dig.scummvm</path><name>Dig</name></game>"
                                                   "<game><path>./Other.zip</path><name>Other</name></game>" )
   {
      writeFile( path( "gl/gamelist.xml" ), "<?xml version=\"1.0\"?>\n<gameList>" + aGames + "</gameList>\n" );
      writeFile( path( "roms/snes/Sub/Game.zip" ), "rom" );
      writeFile( path( "roms/snes/Sub/Game.7z" ), "rom" );
      writeFile( path( "roms/snes/dig.scummvm/dig.001" ), "rom" );
      writeFile( path( "roms/snes/Other.zip" ), "rom" );
      writeFile( path( "media/snes/covers/Sub/Game.jpg" ), "jpg" );
      writeFile( path( "media/snes/videos/Sub/Game.mp4" ), "mp4" );
      writeFile( path( "media/snes/screenshots/Sub/Game.png" ), "png" );
      writeFile( path( "media/snes/manuals/Sub/Game.pdf" ), "pdf" );
      writeFile( path( "media/snes/covers/Other.jpeg" ), "jpeg" );
      system = { QStringLiteral( "snes" ), skSNES, path( "gl/gamelist.xml" ), path( "roms/snes" ),
                 path( "media/snes" ) };
   }

   QString path( const char* aRelative ) const { return dir.filePath( QLatin1String( aRelative ) ); }
};

QImage tinyPicture()
{
   QImage picture( 4, 4, QImage::Format_RGB32 );
   picture.fill( Qt::red );
   return picture;
}

TEST( GamelistEsde, MediaIsFoundByTheRomNameAndTheLinkTagsAreIgnored )
{
   EsdeLayout esde;
   Gamelist list;
   ASSERT_TRUE( list.load( esde.system ) );

   EXPECT_TRUE( list.conventionalMedia() );
   EXPECT_EQ( list.at( 0 ).physicalImagePath, esde.path( "media/snes/covers/Sub/Game.jpg" ) );
   EXPECT_EQ( list.at( 0 ).physicalVideoPath, esde.path( "media/snes/videos/Sub/Game.mp4" ) );
   EXPECT_FALSE( list.at( 0 ).missingImage );
   EXPECT_TRUE( list.at( 0 ).imagePath.isEmpty() ) << "the legacy <image> must not be read";
   EXPECT_TRUE( list.at( 1 ).missingImage );
   EXPECT_EQ( list.at( 1 ).physicalImagePath, esde.path( "media/snes/covers/dig.scummvm.png" ) )
      << "a folder ROM keeps its whole name";
   EXPECT_TRUE( list.at( 2 ).missingImage ) << ".jpeg is not an extension ES-DE looks for";
   EXPECT_EQ( list.at( 2 ).physicalImagePath, esde.path( "media/snes/covers/Other.png" ) );
}

TEST( GamelistEsde, SetImageWritesCoversDropsTheOtherFormatAndLeavesTheGamelistAlone )
{
   EsdeLayout esde;
   Gamelist list;
   ASSERT_TRUE( list.load( esde.system ) );
   const QByteArray before = readAll( esde.system.gamelistPath );

   const Result<> set = list.setImage( 0, tinyPicture() );
   ASSERT_TRUE( set ) << set.error().toStdString();

   EXPECT_EQ( list.at( 0 ).physicalImagePath, esde.path( "media/snes/covers/Sub/Game.png" ) );
   EXPECT_TRUE( QFileInfo::exists( list.at( 0 ).physicalImagePath ) );
   EXPECT_FALSE( QFileInfo::exists( esde.path( "media/snes/covers/Sub/Game.jpg" ) ) )
      << "the jpg would hide the new png";
   EXPECT_EQ( readAll( esde.system.gamelistPath ), before );
   EXPECT_EQ( list.domText( 0, "image" ), QStringLiteral( "./images/legacy.png" ) );
}

TEST( GamelistEsde, SetImageForAFolderRomKeepsTheFolderNameAndAddsNoTag )
{
   EsdeLayout esde;
   Gamelist list;
   ASSERT_TRUE( list.load( esde.system ) );

   ASSERT_TRUE( list.setImage( 1, tinyPicture() ) );

   EXPECT_TRUE( QFileInfo::exists( esde.path( "media/snes/covers/dig.scummvm.png" ) ) );
   EXPECT_TRUE( list.domText( 1, "image" ).isEmpty() );
   EXPECT_FALSE( list.at( 1 ).missingImage );
}

TEST( GamelistEsde, SetVideoReplacesAVideoOfAnotherFormat )
{
   EsdeLayout esde;
   writeFile( esde.path( "media/snes/videos/Other.mkv" ), "mkv" );
   writeFile( esde.path( "source.mp4" ), "new" );
   Gamelist list;
   ASSERT_TRUE( list.load( esde.system ) );

   ASSERT_TRUE( list.setVideo( 2, esde.path( "source.mp4" ) ) );

   EXPECT_TRUE( QFileInfo::exists( esde.path( "media/snes/videos/Other.mp4" ) ) );
   EXPECT_FALSE( QFileInfo::exists( esde.path( "media/snes/videos/Other.mkv" ) ) );
}

TEST( GamelistEsde, RemoveImageDeletesTheFileAndReportsTheExpectedPath )
{
   EsdeLayout esde;
   Gamelist list;
   ASSERT_TRUE( list.load( esde.system ) );

   ASSERT_TRUE( list.removeImage( 0 ) );

   EXPECT_FALSE( QFileInfo::exists( esde.path( "media/snes/covers/Sub/Game.jpg" ) ) );
   EXPECT_TRUE( list.at( 0 ).missingImage );
   EXPECT_EQ( list.at( 0 ).physicalImagePath, esde.path( "media/snes/covers/Sub/Game.png" ) );
}

TEST( GamelistEsde, RemoveGameDeletesItsMediaInEveryTypeFolder )
{
   EsdeLayout esde;
   Gamelist list;
   ASSERT_TRUE( list.load( esde.system ) );

   const Result<QStringList> removed = list.removeGame( 0 );

   ASSERT_TRUE( removed ) << removed.error().toStdString();
   EXPECT_TRUE( removed.value().isEmpty() );
   for ( const char* file : { "roms/snes/Sub/Game.zip", "media/snes/covers/Sub/Game.jpg",
                              "media/snes/videos/Sub/Game.mp4", "media/snes/screenshots/Sub/Game.png",
                              "media/snes/manuals/Sub/Game.pdf" } )
      EXPECT_FALSE( QFileInfo::exists( esde.path( file ) ) ) << file;
   EXPECT_TRUE( QFileInfo::exists( esde.path( "media/snes/covers/Other.jpeg" ) ) );
}

TEST( GamelistEsde, RemoveGameKeepsMediaAnotherRomWithTheSameStemStillUses )
{
   EsdeLayout esde( "<game><path>./Sub/Game.zip</path></game><game><path>./Sub/Game.7z</path></game>" );
   Gamelist list;
   ASSERT_TRUE( list.load( esde.system ) );

   ASSERT_TRUE( list.removeGame( 0 ) );

   EXPECT_FALSE( QFileInfo::exists( esde.path( "roms/snes/Sub/Game.zip" ) ) );
   EXPECT_TRUE( QFileInfo::exists( esde.path( "media/snes/covers/Sub/Game.jpg" ) ) );
}

TEST( GamelistEsde, LinkingAndRenamingMediaAreRefused )
{
   EsdeLayout esde;
   Gamelist list;
   ASSERT_TRUE( list.load( esde.system ) );

   EXPECT_FALSE( list.linkMedia( { { 0, true, esde.path( "media/snes/covers/Other.jpeg" ) } } ) );
   EXPECT_FALSE( list.renameMedia( { { 0, true, QString(), QString(), QString() } } ) );
}

TEST( GamelistCopyMedia, LegacyKeepsTheFormatOfAPictureAndOfAVideo )
{
   Fixture fixture;
   writeFile( fixture.path( "src/a.jpg" ), "jpg bytes" );
   writeFile( fixture.path( "src/b.webm" ), "webm bytes" );
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   const Result<MediaCopyReport> copied =
      list.copyMedia( { { 2, true, fixture.path( "src/a.jpg" ) }, { 2, false, fixture.path( "src/b.webm" ) } } );

   ASSERT_TRUE( copied ) << copied.error().toStdString();
   EXPECT_EQ( copied.value().copied, 2 );
   EXPECT_TRUE( copied.value().skipped.isEmpty() );
   const Gamelist saved = reload( fixture );
   EXPECT_EQ( saved.at( 2 ).imagePath, QStringLiteral( "./media/images/Vanished.jpg" ) );
   EXPECT_EQ( saved.at( 2 ).videoPath, QStringLiteral( "./media/videos/Vanished.webm" ) );
   EXPECT_EQ( readAll( fixture.path( "snes/media/images/Vanished.jpg" ) ), QByteArray( "jpg bytes" ) );
   EXPECT_EQ( readAll( fixture.path( "snes/media/videos/Vanished.webm" ) ), QByteArray( "webm bytes" ) );
}

TEST( GamelistCopyMedia, EsdeKeepsJpgAsIsAndConvertsWhatItCannotRead )
{
   EsdeLayout esde;
   writeFile( esde.path( "src/a.JPG" ), "jpg bytes" );
   // PNG content under a name ES-DE does not look for: QImage sniffs the content.
   ASSERT_TRUE( tinyPicture().save( esde.path( "src/b.jpeg" ), "PNG" ) );
   Gamelist list;
   ASSERT_TRUE( list.load( esde.system ) );
   const QByteArray gamelist = readAll( esde.system.gamelistPath );

   const Result<MediaCopyReport> copied = list.copyMedia(
      { { 2, true, esde.path( "src/a.JPG" ) }, { 1, true, esde.path( "src/b.jpeg" ) } } );

   ASSERT_TRUE( copied ) << copied.error().toStdString();
   EXPECT_EQ( copied.value().copied, 2 );
   EXPECT_EQ( readAll( esde.path( "media/snes/covers/Other.jpg" ) ), QByteArray( "jpg bytes" ) )
      << "lowercase extension, same bytes";
   EXPECT_TRUE( QFileInfo::exists( esde.path( "media/snes/covers/dig.scummvm.png" ) ) );
   EXPECT_FALSE( QFileInfo::exists( esde.path( "media/snes/covers/dig.scummvm.jpeg" ) ) );
   EXPECT_FALSE( list.at( 2 ).missingImage );
   EXPECT_EQ( readAll( esde.system.gamelistPath ), gamelist ) << "ES-DE media never touches the gamelist";
}

TEST( GamelistCopyMedia, EsdeReplacesAVideoOfAnotherFormatAndReportsOneItCannotPlay )
{
   EsdeLayout esde;
   writeFile( esde.path( "src/a.MKV" ), "mkv bytes" );
   writeFile( esde.path( "src/b.flv" ), "flv bytes" );
   Gamelist list;
   ASSERT_TRUE( list.load( esde.system ) );

   const Result<MediaCopyReport> copied = list.copyMedia(
      { { 0, false, esde.path( "src/a.MKV" ) }, { 2, false, esde.path( "src/b.flv" ) } } );

   ASSERT_TRUE( copied ) << copied.error().toStdString();
   EXPECT_EQ( copied.value().copied, 1 );
   ASSERT_EQ( copied.value().skipped.size(), 1 );
   EXPECT_TRUE( copied.value().skipped.first().contains( QStringLiteral( "b.flv" ) ) );
   EXPECT_EQ( readAll( esde.path( "media/snes/videos/Sub/Game.mkv" ) ), QByteArray( "mkv bytes" ) );
   EXPECT_FALSE( QFileInfo::exists( esde.path( "media/snes/videos/Sub/Game.mp4" ) ) )
      << "the mp4 would hide the mkv";
   EXPECT_FALSE( QFileInfo::exists( esde.path( "media/snes/videos/Other.flv" ) ) );
}

TEST( GamelistCopyMedia, AFailedSaveRollsBackEveryFileOfTheBatch )
{
   Fixture fixture;
   writeFile( fixture.path( "src/a.jpg" ), "new jpg" );
   writeFile( fixture.path( "src/b.mp4" ), "new mp4" );
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   const Game before = list.at( 2 );
   breakSave( fixture, list );

   const Result<MediaCopyReport> failed =
      list.copyMedia( { { 2, true, fixture.path( "src/a.jpg" ) }, { 0, false, fixture.path( "src/b.mp4" ) } } );

   EXPECT_FALSE( failed );
   EXPECT_FALSE( QFileInfo::exists( fixture.path( "snes/media/images/Vanished.jpg" ) ) );
   EXPECT_EQ( readAll( fixture.path( "snes/media/videos/Sonic.mp4" ) ), QByteArray( "mp4" ) )
      << "the replaced video must come back";
   EXPECT_FALSE( QFileInfo::exists( fixture.path( "snes/media/videos/Sonic.mp4.gle-bak" ) ) );
   EXPECT_EQ( list.at( 2 ).imagePath, before.imagePath );
   EXPECT_EQ( list.domText( 2, "image" ), before.imagePath );
}

TEST( GamelistCopyMedia, ProgressRunsBeforeEachFileAndCancelingRollsBackTheFilesAlreadyCopied )
{
   Fixture fixture;
   writeFile( fixture.path( "src/a.jpg" ), "new jpg" );
   writeFile( fixture.path( "src/b.mp4" ), "new mp4" );
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   const Game before = list.at( 2 );
   const QVector<MediaCopy> copies{ { 2, true, fixture.path( "src/a.jpg" ) }, { 0, false, fixture.path( "src/b.mp4" ) } };

   int asked = 0;
   const Result<MediaCopyReport> canceled = list.copyMedia( copies, [&asked]( const MediaCopy& ) { return ++asked < 2; } );

   EXPECT_FALSE( canceled );
   EXPECT_EQ( asked, 2 );
   EXPECT_FALSE( QFileInfo::exists( fixture.path( "snes/media/images/Vanished.jpg" ) ) ) << "the first file is taken back";
   EXPECT_EQ( readAll( fixture.path( "snes/media/videos/Sonic.mp4" ) ), QByteArray( "mp4" ) );
   EXPECT_EQ( list.at( 2 ).imagePath, before.imagePath );

   asked = 0;
   const Result<MediaCopyReport> done = list.copyMedia( copies, [&asked]( const MediaCopy& ) { return ++asked > 0; } );
   ASSERT_TRUE( done ) << done.error().toStdString();
   EXPECT_EQ( asked, 2 );
}

}  // namespace
