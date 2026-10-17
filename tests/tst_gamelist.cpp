// Gamelist: the root scan, the parse, and the filter predicate. Everything here runs
// against a real gamelist.xml written into a QTemporaryDir, because the three
// filesystem-backed flags are the point.

#include "GamelistFilter.h"
#include "GamelistFixture.h"
#include "QtPrinters.h"

#include <gtest/gtest.h>

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

   list.setFields( { 0 }, values, which );
   const Result<> written = list.save();
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
   list.setFields( { 2 }, values, filledFields( values ) );
   ASSERT_TRUE( list.save() );

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

   list.setFields( { 0, 1, 2 }, values, filledFields( values ) );
   ASSERT_TRUE( list.save() );

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
   list.setFields( { 0 }, values, changedFields( list.at( 0 ), values ) );

   EXPECT_TRUE( list.at( 0 ).releaseDate.isEmpty() );
   ASSERT_TRUE( list.save() );
   EXPECT_TRUE( reload( fixture ).at( 0 ).releaseDate.isEmpty() );

   // And a good one still converts on the way out.
   values.releaseDate = QStringLiteral( "06/1991" );
   list.setFields( { 0 }, values, GameFieldSet( gfReleaseDate ) );
   ASSERT_TRUE( list.save() );
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

   // Both Sonics are called Sonic.zip, so both links are ./media/images/Sonic.png.
   ASSERT_TRUE( list.setImage( 0, QStringLiteral( ":/DefaultPictures/default.png" ) ) );
   ASSERT_TRUE( list.setImage( 1, QStringLiteral( ":/DefaultPictures/default.png" ) ) );
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

TEST( Gamelist, RemoveGamesDropsOnlyTheEntries )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   // Game 2 (Vanished.zip) is the one orphan; Sonic.zip in two folders is two games.
   EXPECT_EQ( list.orphanIndexes(), QVector<int>( { 2 } ) );
   EXPECT_TRUE( list.duplicateIndexes().isEmpty() );

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

   EXPECT_EQ( list.duplicateIndexes(), QVector<int>( { 2, 3 } ) );

   ASSERT_TRUE( list.removeGames( list.duplicateIndexes() ) );
   EXPECT_EQ( list.count(), 2 );
   EXPECT_EQ( list.at( 0 ).name, QStringLiteral( "A" ) );
   EXPECT_EQ( list.at( 1 ).name, QStringLiteral( "B" ) );
   EXPECT_TRUE( list.duplicateIndexes().isEmpty() );
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
   EXPECT_EQ( list.unlistedRoms(), QStringList( { fixture.path( "snes/Streets.zip" ),
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
   EXPECT_TRUE( list.unlistedRoms().isEmpty() );

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

}  // namespace
