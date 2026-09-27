// Gamelist: the root scan, the parse, and the filter predicate that replaces
// LoadGamesList. Everything here runs against a real gamelist.xml written into a
// QTemporaryDir, because the three filesystem-backed flags are the point.

#include "GamelistFixture.h"
#include "QtPrinters.h"

#include <gtest/gtest.h>

namespace {

TEST( ScanSystems, FindsExtensionlessFolders )
{
   Fixture fixture;

   const QVector<SystemEntry> systems = scanSystems( fixture.dir.path() );

   // Trap: FindFirst( '*.*' ) found these on Windows and would find none here.
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
   QString error;

   ASSERT_TRUE( list.load( fixture.snes, &error ) ) << error.toStdString();

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

   // Absent nodes read as empty rather than raising - F_Main 1839/1896 did not.
   EXPECT_TRUE( list.at( 2 ).imagePath.isEmpty() );
   EXPECT_TRUE( list.at( 2 ).physicalImagePath.isEmpty() );
}

TEST( Gamelist, NodesStayAlignedWithGames )
{
   Fixture fixture;
   Gamelist list;

   ASSERT_TRUE( list.load( fixture.snes ) );

   for ( int index = 0; index < list.count(); ++index ) {
      const QDomElement node = list.gameNode( index );
      ASSERT_FALSE( node.isNull() );
      EXPECT_EQ( node.firstChildElement( QStringLiteral( "path" ) ).text(),
                 list.at( index ).romPath );
   }
}

// Traps: the ROM and the picture are on disk under a different case than
// the gamelist spells them. On Windows that always worked; here it has to be
// resolved, or every such game is an orphan - and "delete orphans from gamelist"
// would then offer to delete the user's library.
TEST( Gamelist, ResolvesPathsWhoseCaseDoesNotMatchTheDisk )
{
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

// The Delphi dropped an unreadable or gameless system silently, leaving the
// document active and the cursor on the hourglass.
TEST( Gamelist, ReportsWhatItCannotLoad )
{
   Fixture fixture;
   Gamelist list;
   QString error;

   EXPECT_FALSE( list.load( { QStringLiteral( "x" ), skOther, fixture.path( "snes/absent.xml" ) }, &error ) );
   EXPECT_FALSE( error.isEmpty() );

   writeFile( fixture.path( "broken/gamelist.xml" ), "<gameList><game>" );
   error.clear();
   EXPECT_FALSE( list.load( { QStringLiteral( "broken" ), skOther, fixture.path( "broken/gamelist.xml" ) }, &error ) );
   EXPECT_TRUE( error.contains( QStringLiteral( "not valid XML" ) ) ) << error.toStdString();

   writeFile( fixture.path( "empty/gamelist.xml" ), "<gameList/>" );
   error.clear();
   EXPECT_FALSE( list.load( { QStringLiteral( "empty" ), skOther, fixture.path( "empty/gamelist.xml" ) }, &error ) );
   EXPECT_TRUE( error.contains( QStringLiteral( "no game" ) ) ) << error.toStdString();
}

TEST( Gamelist, FiltersOnTheGameItself )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   const auto matches = [&list]( int aFilterIndex ) {
      QVector<int> kept;
      FilterSpec filter;
      filter.index = aFilterIndex;
      for ( int index = 0; index < list.count(); ++index )
         if ( list.accepts( index, filter ) )
            kept.append( index );
      return kept;
   };

   EXPECT_EQ( matches( 0 ), QVector<int>( { 0, 1, 2 } ) );
   EXPECT_EQ( matches( 1 ), QVector<int>( { 1, 2 } ) );   // missing picture
   EXPECT_EQ( matches( 2 ), QVector<int>( { 1, 2 } ) );   // missing video
   EXPECT_EQ( matches( 3 ), QVector<int>( { 2 } ) );      // no release date
   EXPECT_EQ( matches( 7 ), QVector<int>( { 1 } ) );      // no publisher
   EXPECT_EQ( matches( 11 ), QVector<int>( { 1 } ) );     // kid game
   EXPECT_EQ( matches( 13 ), QVector<int>( { 0 } ) );     // favorite
   EXPECT_EQ( matches( 14 ), QVector<int>( { 2 } ) );     // orphan
   EXPECT_EQ( matches( 29 ), QVector<int>( { 0, 1 } ) );  // duplicate names
   EXPECT_EQ( matches( 30 ), QVector<int>( { 0, 1 } ) );  // duplicate rom names
}

TEST( Gamelist, SameAsSelectedNeedsASelection )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   FilterSpec filter;
   filter.index = 23;  // same publisher

   // No reference game: the Delphi's Assigned(ReferenceGame) guard, so nothing.
   EXPECT_FALSE( list.accepts( 0, filter ) );

   filter.reference = 0;
   EXPECT_TRUE( list.accepts( 2, filter ) );  // Sega, like the reference
   EXPECT_FALSE( list.accepts( 1, filter ) );

   // Ratings compare numerically, and "0,8" is the same number as "0.8".
   filter.index = 21;  // rating <= reference
   EXPECT_TRUE( list.accepts( 1, filter ) );
   EXPECT_FALSE( list.accepts( 2, filter ) );  // no rating at all, so never
   filter.index = 22;                           // rating >= reference
   EXPECT_FALSE( list.accepts( 1, filter ) );

   // Release date <= reference, over the shortened display forms: "1991" is the
   // 1st of January, which is before the reference's 23rd of June.
   filter.index = 17;
   EXPECT_TRUE( list.accepts( 1, filter ) );
   filter.index = 18;
   EXPECT_FALSE( list.accepts( 1, filter ) );

   // Same folder, and same ROM file name across different folders.
   filter.index = 26;
   EXPECT_TRUE( list.accepts( 2, filter ) );
   EXPECT_FALSE( list.accepts( 1, filter ) );
   filter.index = 28;
   EXPECT_TRUE( list.accepts( 1, filter ) );
}

TEST( Gamelist, SearchMatchesWhateverTheListShows )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   FilterSpec filter;
   EXPECT_EQ( list.displayName( 0, filter ), QStringLiteral( "Sonic The Hedgehog" ) );

   filter.listByRom = true;
   EXPECT_EQ( list.displayName( 1, filter ), QStringLiteral( "Sonic.zip" ) );

   filter.fullRomName = true;
   EXPECT_EQ( list.displayName( 1, filter ), QStringLiteral( "sub/Sonic.zip" ) );

   // ContainsText was case-insensitive, and only the rom path holds "sub".
   filter.search = QStringLiteral( "SUB/" );
   EXPECT_TRUE( list.accepts( 1, filter ) );
   EXPECT_FALSE( list.accepts( 0, filter ) );

   filter.listByRom = false;
   EXPECT_FALSE( list.accepts( 1, filter ) );

   // The search narrows the category; it does not widen it.
   filter.index = 14;  // orphan
   filter.search = QStringLiteral( "sonic" );
   EXPECT_FALSE( list.accepts( 0, filter ) );
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
   for ( const QString& stored : { QStringLiteral( "19910623T000000" ),
                                    QStringLiteral( "19910600T000000" ),
                                    QStringLiteral( "19910000T000000" ) } ) {
      EXPECT_EQ( displayDateToGamelist( gamelistDateToDisplay( stored ) ), stored );
   }
}

// --- write ------------------------------------------------------

// What a second reader sees after a save, which is the only thing that matters:
// the Delphi's reload sites all went back to the file.
Gamelist reload( const Fixture& aFixture )
{
   Gamelist list;
   EXPECT_TRUE( list.load( aFixture.snes ) );
   return list;
}

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
   QString error;
   ASSERT_TRUE( list.save( &error ) ) << error.toStdString();

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

// The node did not exist: NodeExists + AddChild, three copies of it in the
// Delphi, and the reason an absent <image> could crash DeleteGamePicture.
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
   EXPECT_EQ( saved.gameNode( 2 ).firstChildElement( QStringLiteral( "hidden" ) ).text(),
              QStringLiteral( "true" ) );
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
// accepted forms is saved as blank
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

   QString error;
   // Out of the .qrc, which is the reason this is a load-and-save and not a copy.
   ASSERT_TRUE( list.setImage( 2, QStringLiteral( ":/DefaultPictures/default.png" ), &error ) )
      << error.toStdString();

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
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   const QString kept = list.at( 0 ).physicalImagePath;
   QString error;

   // Two of the three have no picture. Btn_ChangeAll was captioned for exactly
   // this and had no such test, so it overwrote the third one's art as well.
   EXPECT_EQ( list.setDefaultImageForMissing( QStringLiteral( ":/DefaultPictures/default.png" ),
                                               &error ),
              2 );
   EXPECT_TRUE( error.isEmpty() ) << error.toStdString();

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

TEST( Gamelist, SetAndRemoveVideo )
{
   Fixture fixture;
   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   writeFile( fixture.path( "elsewhere/clip.mp4" ), "video" );
   QString error;
   ASSERT_TRUE( list.setVideo( 2, fixture.path( "elsewhere/clip.mp4" ), &error ) )
      << error.toStdString();

   EXPECT_EQ( list.at( 2 ).videoPath, QStringLiteral( "./media/videos/Vanished.mp4" ) );
   EXPECT_TRUE( QFileInfo::exists( fixture.path( "snes/media/videos/Vanished.mp4" ) ) );

   // Choosing the game's own video must not be remove-then-copy onto itself.
   ASSERT_TRUE( list.setVideo( 2, list.at( 2 ).physicalVideoPath, &error ) )
      << error.toStdString();
   EXPECT_EQ( QFile( list.at( 2 ).physicalVideoPath ).size(), 5 );

   ASSERT_TRUE( list.removeVideo( 2 ) );
   EXPECT_FALSE( QFileInfo::exists( fixture.path( "snes/media/videos/Vanished.mp4" ) ) );
   EXPECT_TRUE( reload( fixture ).at( 2 ).videoPath.isEmpty() );
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

   // GetFileSize raised on a ROM that had vanished; this just comes back empty.
   list.ensureHashes( 2 );
   EXPECT_TRUE( list.at( 2 ).crc32.isEmpty() );
}

}  // namespace
