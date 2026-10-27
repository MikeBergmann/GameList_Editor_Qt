// LaunchBoxDb: name key, indexing one platform from plain XML, lookup order and field mapping.

#include "LaunchBoxDb.h"

#include "QtPrinters.h"

#include <gtest/gtest.h>

#include <QBuffer>

namespace {

const QString Nes = QStringLiteral( "Nintendo Entertainment System" );
const QString Arcade = QStringLiteral( "Arcade" );

const QByteArray kMetadata = QByteArrayLiteral( R"xml(<?xml version="1.0" standalone="yes"?>
<LaunchBox>
  <Platform><Name>Nintendo Entertainment System</Name></Platform>
  <Game>
    <DatabaseID>1</DatabaseID>
    <Name>The Legend of Zelda</Name>
    <Platform>Nintendo Entertainment System</Platform>
    <ReleaseDate>1987-02-21</ReleaseDate>
    <ReleaseYear>1987</ReleaseYear>
    <Overview>Find the Triforce.</Overview>
    <MaxPlayers>1</MaxPlayers>
    <Genres>Action; Adventure </Genres>
    <Developer>Nintendo</Developer>
    <Publisher>Nintendo</Publisher>
    <CommunityRating>4.0</CommunityRating>
  </Game>
  <Game>
    <DatabaseID>2</DatabaseID>
    <Name>Tom &amp; Jerry: Frantic Antics</Name>
    <Platform>Nintendo Entertainment System</Platform>
    <ReleaseDate></ReleaseDate>
    <ReleaseYear>1993</ReleaseYear>
    <Genres></Genres>
  </Game>
  <Game>
    <DatabaseID>3</DatabaseID>
    <Name>Duplicate</Name>
    <Platform>Nintendo Entertainment System</Platform>
    <Overview>first</Overview>
  </Game>
  <Game>
    <DatabaseID>4</DatabaseID>
    <Name>Duplicate!</Name>
    <Platform>Nintendo Entertainment System</Platform>
    <Overview>second</Overview>
  </Game>
  <Game>
    <DatabaseID>5</DatabaseID>
    <Name>Sonic the Hedgehog</Name>
    <Platform>Sega Genesis</Platform>
  </Game>
  <Game>
    <DatabaseID>6</DatabaseID>
    <Name>Super Mario Bros.</Name>
    <Platform>Nintendo Entertainment System</Platform>
    <ReleaseDate>2000-08-11T00:00:00-07:00</ReleaseDate>
    <CommunityRating>0.5</CommunityRating>
  </Game>
  <Game>
    <DatabaseID>7</DatabaseID>
    <Name>Street Fighter II: The World Warrior</Name>
    <Platform>Arcade</Platform>
  </Game>
  <Game>
    <DatabaseID>8</DatabaseID>
    <Name>Hyper Street Fighter II: The Anniversary Edition</Name>
    <Platform>Arcade</Platform>
  </Game>
  <GameAlternateName>
    <DatabaseID>1</DatabaseID>
    <AlternateName>Zelda no Densetsu</AlternateName>
    <Region>Japan</Region>
  </GameAlternateName>
  <GameAlternateName>
    <DatabaseID>6</DatabaseID>
    <AlternateName>Mario Bros Special</AlternateName>
    <Region>Europe</Region>
  </GameAlternateName>
  <GameAlternateName>
    <DatabaseID>5</DatabaseID>
    <AlternateName>Sonic Genesis Alt</AlternateName>
    <Region>Europe</Region>
  </GameAlternateName>
  <GameImage>
    <DatabaseID>1</DatabaseID>
    <FileName>boxj.png</FileName>
    <Type>Box - Front</Type>
    <Region>Japan</Region>
  </GameImage>
  <GameImage>
    <DatabaseID>1</DatabaseID>
    <FileName>boxna.PNG</FileName>
    <Type>Box - Front</Type>
    <Region>North America</Region>
  </GameImage>
  <GameImage>
    <DatabaseID>1</DatabaseID>
    <FileName>box3dna.jpg</FileName>
    <Type>Box - 3D</Type>
    <Region>North America</Region>
  </GameImage>
  <GameImage>
    <DatabaseID>1</DatabaseID>
    <FileName>box3dj.jpg</FileName>
    <Type>Box - 3D</Type>
    <Region>Japan</Region>
  </GameImage>
  <GameImage>
    <DatabaseID>1</DatabaseID>
    <FileName>shot.jpg</FileName>
    <Type>Screenshot - Gameplay</Type>
  </GameImage>
  <GameImage>
    <DatabaseID>1</DatabaseID>
    <FileName>logo.png</FileName>
    <Type>Clear Logo</Type>
    <Region>World</Region>
  </GameImage>
  <GameImage>
    <DatabaseID>5</DatabaseID>
    <FileName>sonic.png</FileName>
    <Type>Box - Front</Type>
  </GameImage>
  <GameImage>
    <DatabaseID>6</DatabaseID>
    <FileName>mario-recon.png</FileName>
    <Type>Box - Front - Reconstructed</Type>
    <Region>Europe</Region>
  </GameImage>
</LaunchBox>
)xml" );

const QByteArray kFiles = QByteArrayLiteral( R"xml(<LaunchBox>
  <File>
    <FileName>Hyper Street Fighter II (USA 040202)</FileName>
    <Platform>Arcade</Platform>
    <GameName>Hyper Street Fighter II: The Anniversary Edition (USA 040202)</GameName>
  </File>
  <File>
    <FileName>Hyper Street Fighter II (USA 040202)</FileName>
    <Platform>Sega Genesis</Platform>
    <GameName>Wrong Platform</GameName>
  </File>
</LaunchBox>
)xml" );

const QByteArray kMame = QByteArrayLiteral( R"xml(<LaunchBox>
  <MameFile>
    <FileName>sf2</FileName>
    <Name>Street Fighter II: The World Warrior</Name>
  </MameFile>
  <MameListItem><FileName>sf2</FileName><GameName>ignored</GameName></MameListItem>
</LaunchBox>
)xml" );

Result<> load( LaunchBoxDb& aDb, const QByteArray& aXml, const QString& aPlatform )
{
   QBuffer buffer;
   buffer.setData( aXml );
   buffer.open( QIODevice::ReadOnly );
   return aDb.loadMetadata( buffer, aPlatform );
}

LaunchBoxDb nesDb()
{
   LaunchBoxDb db;
   const Result<> loaded = load( db, kMetadata, Nes );
   EXPECT_TRUE( loaded ) << loaded.error().toStdString();
   return db;
}

LaunchBoxQuery query( const QString& aRom, const QString& aName = {},
                      const QString& aRegion = {}, const QSet<QString>& aTypes = {} )
{
   LaunchBoxQuery q;
   q.romName = aRom;
   q.gameName = aName;
   q.region = aRegion;
   q.mediaTypes = aTypes;
   return q;
}

const QSet<QString> kAllTypes = { QStringLiteral( "box-2D" ), QStringLiteral( "box-3D" ),
                                  QStringLiteral( "ss" ), QStringLiteral( "sstitle" ),
                                  QStringLiteral( "wheel" ), QStringLiteral( "mixrbv1" ) };

}  // namespace

TEST( LaunchBoxNameKey, DropsTagsAccentsAndPunctuation )
{
   EXPECT_EQ( launchBoxNameKey( QStringLiteral( "Super Mario Bros. (USA) [!] {x}" ) ),
              QStringLiteral( "SUPER_MARIO_BROS" ) );
   EXPECT_EQ( launchBoxNameKey( QStringLiteral( "Pok\u00E9mon" ) ), QStringLiteral( "POKEMON" ) );
   EXPECT_EQ( launchBoxNameKey( QStringLiteral( "Tom & Jerry" ) ),
              QStringLiteral( "TOM_AND_JERRY" ) );
   EXPECT_EQ( launchBoxNameKey( QStringLiteral( "Tom&Jerry" ) ), QStringLiteral( "TOM_AND_JERRY" ) )
      << "an ampersand without spaces must still be its own word";
   EXPECT_EQ( launchBoxNameKey( QStringLiteral( "Bart's Nightmare?" ) ),
              QStringLiteral( "BARTS_NIGHTMARE" ) );
   EXPECT_EQ( launchBoxNameKey( QStringLiteral( "Zelda II - The  Adventure: of Link" ) ),
              QStringLiteral( "ZELDA_II_THE_ADVENTURE_OF_LINK" ) )
      << "':' and '-' and runs of spaces collapse to one '_'";
   EXPECT_EQ( launchBoxNameKey( QStringLiteral( "Foo -" ) ), QStringLiteral( "FOO" ) );
   EXPECT_EQ( launchBoxNameKey( QStringLiteral( "(USA)" ) ), QString() );
}

TEST( LaunchBoxDb, IndexesOnlyTheRequestedPlatform )
{
   const LaunchBoxDb nes = nesDb();
   EXPECT_EQ( nes.gameCount(), 5 );
   EXPECT_FALSE( nes.find( query( QStringLiteral( "Sonic the Hedgehog" ) ) ) );

   LaunchBoxDb genesis;
   ASSERT_TRUE( load( genesis, kMetadata, QStringLiteral( "Sega Genesis" ) ) );
   EXPECT_EQ( genesis.gameCount(), 1 );
   EXPECT_TRUE( genesis.find( query( QStringLiteral( "Sonic the Hedgehog" ) ) ) );
}

TEST( LaunchBoxDb, FindsByRomNameThenGameName )
{
   const LaunchBoxDb db = nesDb();

   const auto byRom = db.find( query( QStringLiteral( "Super Mario Bros. (World)" ) ) );
   ASSERT_TRUE( byRom );
   EXPECT_EQ( byRom.value().info.name, QStringLiteral( "Super Mario Bros." ) );

   const auto byName =
      db.find( query( QStringLiteral( "smb1" ), QStringLiteral( "Super Mario Bros." ) ) );
   ASSERT_TRUE( byName );
   EXPECT_EQ( byName.value().info.name, QStringLiteral( "Super Mario Bros." ) );
}

TEST( LaunchBoxDb, MovesATrailingArticleToTheFront )
{
   const LaunchBoxDb db = nesDb();
   const auto found = db.find( query( QStringLiteral( "Legend of Zelda, The (USA)" ) ) );
   ASSERT_TRUE( found );
   EXPECT_EQ( found.value().info.developer, QStringLiteral( "Nintendo" ) );
}

TEST( LaunchBoxDb, FindsByAlternateNameAndUsesItForTheRegion )
{
   const LaunchBoxDb db = nesDb();

   const auto alt = db.find( query( QStringLiteral( "x" ), QStringLiteral( "Mario Bros Special" ) ) );
   ASSERT_TRUE( alt );
   EXPECT_EQ( alt.value().info.name, QStringLiteral( "Super Mario Bros." ) )
      << "no region asked for, so the game's own name stays";

   const auto japan = db.find( query( QStringLiteral( "The Legend of Zelda (Japan)" ) ) );
   ASSERT_TRUE( japan );
   EXPECT_EQ( japan.value().info.name, QStringLiteral( "Zelda no Densetsu" ) );
   EXPECT_EQ( japan.value().info.region, QStringLiteral( "Japan" ) );

   const auto usa = db.find( query( QStringLiteral( "The Legend of Zelda (USA)" ) ) );
   ASSERT_TRUE( usa );
   EXPECT_EQ( usa.value().info.name, QStringLiteral( "The Legend of Zelda" ) );
   EXPECT_TRUE( usa.value().info.region.isEmpty() );
}

TEST( LaunchBoxDb, NotFoundIsAnError )
{
   const LaunchBoxDb db = nesDb();
   const auto missing = db.find( query( QStringLiteral( "No Such Game" ), QStringLiteral( "Nope" ) ) );
   ASSERT_FALSE( missing );
   EXPECT_EQ( missing.error(), QStringLiteral( "Game not found" ) );
}

TEST( LaunchBoxDb, SharedNameKeyTakesTheFirstAndSaysSo )
{
   const LaunchBoxDb db = nesDb();
   const auto found = db.find( query( QStringLiteral( "Duplicate" ) ) );
   ASSERT_TRUE( found );
   EXPECT_TRUE( found.value().ambiguous );
   EXPECT_EQ( found.value().info.description, QStringLiteral( "first" ) );

   const auto unique = db.find( query( QStringLiteral( "Super Mario Bros." ) ) );
   ASSERT_TRUE( unique );
   EXPECT_FALSE( unique.value().ambiguous );
}

TEST( LaunchBoxDb, MapsFields )
{
   const LaunchBoxDb db = nesDb();

   const auto zelda = db.find( query( QStringLiteral( "The Legend of Zelda" ) ) );
   ASSERT_TRUE( zelda );
   const ScrapedInfo& info = zelda.value().info;
   EXPECT_EQ( info.description, QStringLiteral( "Find the Triforce." ) );
   EXPECT_EQ( info.releaseDate, QStringLiteral( "21/02/1987" ) );
   EXPECT_EQ( info.genre, QStringLiteral( "Action - Adventure" ) );
   EXPECT_EQ( info.developer, QStringLiteral( "Nintendo" ) );
   EXPECT_EQ( info.publisher, QStringLiteral( "Nintendo" ) );
   EXPECT_EQ( info.players, QStringLiteral( "1" ) );
   EXPECT_EQ( info.rating, QStringLiteral( "0.80" ) ) << "CommunityRating 0.5-5 to ES's 0-1";
   EXPECT_EQ( info.maxThreads, 2 );
   EXPECT_TRUE( info.videoLink.isEmpty() );

   const auto tom = db.find( query( QStringLiteral( "Tom and Jerry - Frantic Antics" ) ) );
   ASSERT_TRUE( tom );
   EXPECT_EQ( tom.value().info.releaseDate, QStringLiteral( "1993" ) ) << "empty date, ReleaseYear";
   EXPECT_TRUE( tom.value().info.genre.isEmpty() );
   EXPECT_TRUE( tom.value().info.rating.isEmpty() );

   const auto mario = db.find( query( QStringLiteral( "Super Mario Bros." ) ) );
   ASSERT_TRUE( mario );
   EXPECT_EQ( mario.value().info.releaseDate, QStringLiteral( "11/08/2000" ) )
      << "a time and zone offset is dropped";
   EXPECT_EQ( mario.value().info.rating, QStringLiteral( "0.10" ) );
}

TEST( LaunchBoxDb, PicksImagesByTypeAndRegion )
{
   const LaunchBoxDb db = nesDb();
   const QString base = QStringLiteral( "https://images.launchbox-app.com/" );

   const auto japan = db.find( query( QStringLiteral( "The Legend of Zelda (Japan)" ), {}, {}, kAllTypes ) );
   ASSERT_TRUE( japan );
   const QVector<MediaInfo>& pics = japan.value().info.pictures;
   ASSERT_EQ( pics.size(), 4 ) << "box, screenshot, 3D and logo; no title image, no mix";
   EXPECT_EQ( pics[0].fileLink, base + QStringLiteral( "boxj.png" ) );
   EXPECT_EQ( pics[1].fileLink, base + QStringLiteral( "shot.jpg" ) ) << "no region on it: first of type";
   EXPECT_EQ( pics[2].fileLink, base + QStringLiteral( "box3dj.jpg" ) ) << "follows the box's region";
   EXPECT_EQ( pics[3].fileLink, base + QStringLiteral( "logo.png" ) );

   const auto usa = db.find( query( QStringLiteral( "The Legend of Zelda" ), {}, QStringLiteral( "USA" ),
                                    { QStringLiteral( "box-2D" ), QStringLiteral( "box-3D" ) } ) );
   ASSERT_TRUE( usa );
   ASSERT_EQ( usa.value().info.pictures.size(), 2 );
   EXPECT_EQ( usa.value().info.pictures[0].fileLink, base + QStringLiteral( "boxna.PNG" ) );
   EXPECT_EQ( usa.value().info.pictures[0].fileExt, QStringLiteral( "png" ) )
      << "the extension is lower-cased";
   EXPECT_EQ( usa.value().info.pictures[1].fileLink, base + QStringLiteral( "box3dna.jpg" ) );

   const auto none = db.find( query( QStringLiteral( "The Legend of Zelda" ) ) );
   ASSERT_TRUE( none );
   EXPECT_TRUE( none.value().info.pictures.isEmpty() ) << "nothing requested, nothing offered";
}

TEST( LaunchBoxDb, ImageTypeFallbackAndNoRegionMatch )
{
   const LaunchBoxDb db = nesDb();
   const auto mario = db.find( query( QStringLiteral( "Super Mario Bros." ), {},
                                      QStringLiteral( "Japan" ), { QStringLiteral( "box-2D" ) } ) );
   ASSERT_TRUE( mario );
   ASSERT_EQ( mario.value().info.pictures.size(), 1 );
   EXPECT_EQ( mario.value().info.pictures[0].fileLink,
              QStringLiteral( "https://images.launchbox-app.com/mario-recon.png" ) )
      << "no Box - Front: the reconstructed one, though its region is not Japan";
}

TEST( LaunchBoxDb, ArcadeUsesFilesAndMame )
{
   LaunchBoxDb db;
   ASSERT_TRUE( load( db, kMetadata, Arcade ) );
   EXPECT_EQ( db.gameCount(), 2 );

   const LaunchBoxQuery hyper = query( QStringLiteral( "Hyper Street Fighter II (USA 040202)" ) );
   EXPECT_FALSE( db.find( hyper ) ) << "the file name alone does not match the title";

   QBuffer files;
   files.setData( kFiles );
   files.open( QIODevice::ReadOnly );
   ASSERT_TRUE( db.loadFiles( files, Arcade ) );
   const auto viaFiles = db.find( hyper );
   ASSERT_TRUE( viaFiles );
   EXPECT_EQ( viaFiles.value().info.name, QStringLiteral( "Hyper Street Fighter II: The Anniversary Edition" ) );

   const LaunchBoxQuery sf2 = query( QStringLiteral( "sf2" ) );
   EXPECT_FALSE( db.find( sf2 ) );
   QBuffer mame;
   mame.setData( kMame );
   mame.open( QIODevice::ReadOnly );
   ASSERT_TRUE( db.loadMame( mame ) );
   const auto viaMame = db.find( sf2 );
   ASSERT_TRUE( viaMame );
   EXPECT_EQ( viaMame.value().info.name, QStringLiteral( "Street Fighter II: The World Warrior" ) );
}

TEST( LaunchBoxDb, BrokenXmlIsAnErrorAndLeavesNoGames )
{
   LaunchBoxDb db = nesDb();
   const Result<> bad = load( db, kMetadata.left( kMetadata.size() / 2 ), Nes );
   EXPECT_FALSE( bad );
   EXPECT_FALSE( bad.error().isEmpty() );
   EXPECT_EQ( db.gameCount(), 0 );

   EXPECT_FALSE( load( db, QByteArrayLiteral( "not xml" ), Nes ) );
}
