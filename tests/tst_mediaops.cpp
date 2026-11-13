// MediaOps: finding unlinked, misnamed and oversized media. Linking and
// renaming are Gamelist actions; they are exercised here because the finders feed them.

#include "GamelistFixture.h"
#include "MediaOps.h"
#include "QtPrinters.h"

#include <gtest/gtest.h>

namespace {

TEST( MediaOps, UnlinkedMediaFindsFilesByRomNameAndLinkMediaPointsTheGamesAtThem )
{
   Fixture fixture;
   writeFile( fixture.path( "snes/media/images/Vanished-image.jpg" ), "jpg" );

   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   // Game 0 links working files; game 1 has a dead image link and no video, and
   // sonic.PNG / Sonic.mp4 sit there under its ROM name; game 2 has a -image.
   const QVector<MediaLink> found = unlinkedMedia( list );
   ASSERT_EQ( found.size(), 3 );
   EXPECT_EQ( found[0].index, 1 );
   EXPECT_TRUE( found[0].isImage );
   EXPECT_EQ( found[0].path, fixture.path( "snes/media/images/sonic.PNG" ) );
   EXPECT_EQ( found[1].index, 2 );
   EXPECT_EQ( found[1].path, fixture.path( "snes/media/images/Vanished-image.jpg" ) );
   EXPECT_EQ( found[2].index, 1 );
   EXPECT_FALSE( found[2].isImage );

   const Result<int> linked = list.linkMedia( found );
   ASSERT_TRUE( linked ) << linked.error().toStdString();
   ASSERT_EQ( linked.value(), 3 );
   EXPECT_FALSE( list.at( 1 ).missingImage );
   EXPECT_FALSE( list.at( 1 ).missingVideo );
   EXPECT_TRUE( unlinkedMedia( list ).isEmpty() );

   // The link keeps the file's real case, and survives a reload.
   const Gamelist saved = reload( fixture );
   EXPECT_EQ( saved.at( 1 ).imagePath, QStringLiteral( "./media/images/sonic.PNG" ) );
   EXPECT_EQ( saved.at( 1 ).videoPath, QStringLiteral( "./media/videos/Sonic.mp4" ) );
   EXPECT_EQ( saved.at( 2 ).imagePath, QStringLiteral( "./media/images/Vanished-image.jpg" ) );
   EXPECT_EQ( saved.at( 0 ).imagePath, QStringLiteral( "./media/images/Sonic.png" ) );
}

TEST( MediaOps, MisnamedMediaProposesRomNamesAndRenameMediaRelinksThem )
{
   SKIP_ON_CASE_INSENSITIVE_FS();
   Fixture fixture;
   writeFile( fixture.path( "snes/media/images/Sonic.png" ), "png" );
   writeFile( fixture.path( "snes/media/images/Sonic.PNG" ), "png" );  // blocks game 1's rename
   writeFile( fixture.path( "snes/media/images/Renamed.jpg" ), "jpg" );

   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   // Game 1 links sonic.PNG, whose proper name Sonic.PNG is taken; game 2 links Renamed.jpg.
   const Result<int> linked =
      list.linkMedia( { { 1, true, fixture.path( "snes/media/images/sonic.PNG" ) },
                        { 2, true, fixture.path( "snes/media/images/Renamed.jpg" ) } } );
   ASSERT_TRUE( linked ) << linked.error().toStdString();
   ASSERT_EQ( linked.value(), 2 );

   // Game 0's Sonic.png / Sonic.mp4 are already right.
   const QVector<MediaRename> found = misnamedMedia( list );
   ASSERT_EQ( found.size(), 2 );
   EXPECT_EQ( found[0].index, 1 );
   EXPECT_FALSE( found[0].problem.isEmpty() );
   EXPECT_EQ( found[1].index, 2 );
   EXPECT_TRUE( found[1].problem.isEmpty() );
   EXPECT_EQ( found[1].newPath, fixture.path( "snes/media/images/Vanished.jpg" ) );

   const Result<int> renamed = list.renameMedia( found );
   ASSERT_TRUE( renamed ) << renamed.error().toStdString();
   ASSERT_EQ( renamed.value(), 1 );
   EXPECT_FALSE( QFileInfo::exists( fixture.path( "snes/media/images/Renamed.jpg" ) ) );
   EXPECT_TRUE( QFileInfo::exists( fixture.path( "snes/media/images/Vanished.jpg" ) ) );
   EXPECT_TRUE( QFileInfo::exists( fixture.path( "snes/media/images/sonic.PNG" ) ) );
   EXPECT_FALSE( list.at( 2 ).missingImage );

   const Gamelist saved = reload( fixture );
   EXPECT_EQ( saved.at( 2 ).imagePath, QStringLiteral( "./media/images/Vanished.jpg" ) );
   EXPECT_EQ( saved.at( 1 ).imagePath, QStringLiteral( "./media/images/sonic.PNG" ) );

   // Nothing left to propose but the blocked one.
   EXPECT_EQ( misnamedMedia( list ).size(), 1 );
}

TEST( MediaOps, StrictMisnamedMediaAlsoRenamesDashImageAndDashVideoFiles )
{
   SKIP_ON_CASE_INSENSITIVE_FS();
   Fixture fixture;
   writeFile( fixture.path( "snes/media/images/Sonic.png" ), "png" );
   writeFile( fixture.path( "snes/media/images/Vanished-image.jpg" ), "jpg" );
   writeFile( fixture.path( "snes/media/videos/Vanished-video.mp4" ), "mp4" );

   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   const Result<int> linked =
      list.linkMedia( { { 2, true, fixture.path( "snes/media/images/Vanished-image.jpg" ) },
                        { 2, false, fixture.path( "snes/media/videos/Vanished-video.mp4" ) } } );
   ASSERT_TRUE( linked ) << linked.error().toStdString();
   ASSERT_EQ( linked.value(), 2 );

   // Scraper-style names are fine by default ...
   EXPECT_TRUE( misnamedMedia( list ).isEmpty() );

   // ... and wrong when asked for the plain ROM name.
   const QVector<MediaRename> found = misnamedMedia( list, true );
   ASSERT_EQ( found.size(), 2 );
   EXPECT_EQ( found[0].newPath, fixture.path( "snes/media/images/Vanished.jpg" ) );
   EXPECT_EQ( found[1].newPath, fixture.path( "snes/media/videos/Vanished.mp4" ) );

   const Result<int> renamed = list.renameMedia( found );
   ASSERT_TRUE( renamed ) << renamed.error().toStdString();
   ASSERT_EQ( renamed.value(), 2 );
   const Gamelist saved = reload( fixture );
   EXPECT_EQ( saved.at( 2 ).imagePath, QStringLiteral( "./media/images/Vanished.jpg" ) );
   EXPECT_EQ( saved.at( 2 ).videoPath, QStringLiteral( "./media/videos/Vanished.mp4" ) );
   EXPECT_TRUE( misnamedMedia( list, true ).isEmpty() );
}

TEST( MediaOps, UnlinkedMediaAlsoFindsFullRomNamesInPlainImagesAndVideosFolders )
{
   // A ScummVM system: no media links yet, art in ./images and ./videos, named
   // after the whole ROM name.
   Fixture fixture;
   writeFile( fixture.path( "snes/images/Vanished.zip.png" ), "png" );
   writeFile( fixture.path( "snes/videos/Vanished.zip.mp4" ), "mp4" );

   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   bool image = false;
   bool video = false;
   for ( const MediaLink& link : unlinkedMedia( list ) ) {
      if ( link.index != 2 )
         continue;
      ( link.isImage ? image : video ) = true;
      EXPECT_TRUE( link.path.contains( link.isImage ? QStringLiteral( "/images/Vanished.zip.png" )
                                                    : QStringLiteral( "/videos/Vanished.zip.mp4" ) ) );
   }
   EXPECT_TRUE( image );
   EXPECT_TRUE( video );
}

TEST( MediaOps, UnlinkedMediaGoesByMimeTypeNotAnExtensionList )
{
   // .webp and .webm are in no list of ours, and a .txt of the same name is not media.
   Fixture fixture;
   writeFile( fixture.path( "snes/media/images/Vanished.txt" ), "notes" );
   writeFile( fixture.path( "snes/media/images/Vanished.webp" ), "webp" );
   writeFile( fixture.path( "snes/media/videos/Vanished.txt" ), "notes" );
   writeFile( fixture.path( "snes/media/videos/Vanished.webm" ), "webm" );

   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   QStringList paths;
   for ( const MediaLink& link : unlinkedMedia( list ) )
      if ( link.index == 2 )
         paths << link.path;
   paths.sort();
   EXPECT_EQ( paths, QStringList( { fixture.path( "snes/media/images/Vanished.webp" ),
                                    fixture.path( "snes/media/videos/Vanished.webm" ) } ) );
}

TEST( MediaOps, UnlinkedMediaDoesNotSearchTheWorkingDirectoryForAnEmptyMediaFolder )
{
   // A link with no '/' leaves the media folder empty; QDir("") is the working directory.
   Fixture fixture;
   writeFile( fixture.path( "snes/gamelist.xml" ),
              "<gameList><game><path>./Lost.zip</path><image>Lost.png</image></game></gameList>" );
   writeFile( fixture.path( "elsewhere/Lost.png" ), "png" );

   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   const QString previous = QDir::currentPath();
   ASSERT_TRUE( QDir::setCurrent( fixture.path( "elsewhere" ) ) );
   const QVector<MediaLink> found = unlinkedMedia( list );
   QDir::setCurrent( previous );

   EXPECT_TRUE( found.isEmpty() );
}

TEST( MediaOps, OversizedImagesAreFoundOncePerFile )
{
   Fixture fixture;
   writeFile( fixture.path( "snes/media/images/Sonic.png" ), QByteArray( 5000, 'x' ) );

   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );

   // Game 0 links the big file; game 1 links a missing one, game 2 none.
   const QVector<OversizedImage> found = oversizedImages( list, 1000 );
   ASSERT_EQ( found.size(), 1 );
   EXPECT_EQ( found.at( 0 ).index, 0 );
   EXPECT_EQ( found.at( 0 ).size, 5000 );
   EXPECT_TRUE( oversizedImages( list, 5000 ).isEmpty() );
}

// What a failed link or rename must leave behind: the game, and the DOM, as they were.
void expectLinksAre( const Gamelist& aList, int aIndex, const Game& aBefore, const QString& aImageDom,
                     const QString& aVideoDom )
{
   const Game& game = aList.at( aIndex );
   EXPECT_EQ( game.imagePath, aBefore.imagePath );
   EXPECT_EQ( game.videoPath, aBefore.videoPath );
   EXPECT_EQ( game.physicalImagePath, aBefore.physicalImagePath );
   EXPECT_EQ( game.physicalVideoPath, aBefore.physicalVideoPath );
   EXPECT_EQ( game.missingImage, aBefore.missingImage );
   EXPECT_EQ( game.missingVideo, aBefore.missingVideo );
   EXPECT_EQ( aList.domText( aIndex, Cst_ImageLink ), aImageDom );
   EXPECT_EQ( aList.domText( aIndex, Cst_VideoLink ), aVideoDom );
}

TEST( MediaOps, LinkMediaFailedSaveLeavesGamesAndDocumentAsTheyWere )
{
   Fixture fixture;
   writeFile( fixture.path( "snes/media/images/Vanished-image.jpg" ), "jpg" );
   writeFile( fixture.path( "snes/media/videos/Vanished-video.mp4" ), "mp4" );

   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   const Game before1 = list.at( 1 );
   const Game before2 = list.at( 2 );
   const QString image1 = list.domText( 1, Cst_ImageLink );
   const QString video1 = list.domText( 1, Cst_VideoLink );
   const QString image2 = list.domText( 2, Cst_ImageLink );
   const QString video2 = list.domText( 2, Cst_VideoLink );

   breakSave( fixture, list );

   // Game 2 gets an image and a video in one batch: both must come back.
   const Result<int> failed =
      list.linkMedia( { { 1, true, fixture.path( "snes/media/images/sonic.PNG" ) },
                        { 2, true, fixture.path( "snes/media/images/Vanished-image.jpg" ) },
                        { 2, false, fixture.path( "snes/media/videos/Vanished-video.mp4" ) } } );
   EXPECT_FALSE( failed );
   EXPECT_FALSE( failed.error().isEmpty() );
   expectLinksAre( list, 1, before1, image1, video1 );
   expectLinksAre( list, 2, before2, image2, video2 );
}

TEST( MediaOps, RenameMediaFailedSaveRenamesTheFilesBack )
{
   Fixture fixture;
   const QString renamed = fixture.path( "snes/media/images/Renamed.jpg" );
   writeFile( fixture.path( "snes/media/images/Sonic.png" ), "png" );  // game 0's link, in its own case
   writeFile( renamed, "jpg" );

   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   ASSERT_TRUE( list.linkMedia( { { 2, true, renamed } } ) );
   const Game before = list.at( 2 );
   const QString image = list.domText( 2, Cst_ImageLink );
   const QString video = list.domText( 2, Cst_VideoLink );

   const QVector<MediaRename> found = misnamedMedia( list );
   ASSERT_EQ( found.size(), 1 );
   ASSERT_TRUE( found[0].problem.isEmpty() );

   breakSave( fixture, list );

   const Result<int> failed = list.renameMedia( found );
   EXPECT_FALSE( failed );
   EXPECT_FALSE( failed.error().isEmpty() );
   EXPECT_TRUE( QFileInfo::exists( renamed ) );
   EXPECT_FALSE( QFileInfo::exists( found[0].newPath ) );
   expectLinksAre( list, 2, before, image, video );
}

TEST( MediaOps, RenameMediaThatFailsMidwayPutsTheEarlierRenamesBack )
{
   Fixture fixture;
   const QString renamed = fixture.path( "snes/media/images/Renamed.jpg" );
   writeFile( renamed, "jpg" );

   Gamelist list;
   ASSERT_TRUE( list.load( fixture.snes ) );
   ASSERT_TRUE( list.linkMedia( { { 2, true, renamed } } ) );
   const Game before = list.at( 2 );
   const QString image = list.domText( 2, Cst_ImageLink );
   const QString video = list.domText( 2, Cst_VideoLink );

   // The first rename works; the second names a file that is not there.
   const QString target = fixture.path( "snes/media/images/Vanished.jpg" );
   const QVector<MediaRename> renames = {
      { 2, true, renamed, target, QString() },
      { 1, true, fixture.path( "snes/media/images/absent.png" ),
        fixture.path( "snes/media/images/Gone.png" ), QString() },
   };

   const Result<int> failed = list.renameMedia( renames );
   EXPECT_FALSE( failed );
   EXPECT_TRUE( QFileInfo::exists( renamed ) );
   EXPECT_FALSE( QFileInfo::exists( target ) );
   expectLinksAre( list, 2, before, image, video );
}

TEST( MediaOps, VideoCodecNameReadsTheSampleEntryOfAnMp4WhateverTheBoxOrder )
{
   Fixture fixture;
   const auto box = []( const QByteArray& aType, const QByteArray& aBody ) {
      const quint32 size = 8 + static_cast<quint32>( aBody.size() );
      QByteArray head( 4, 0 );
      head[0] = static_cast<char>( size >> 24 );
      head[1] = static_cast<char>( size >> 16 );
      head[2] = static_cast<char>( size >> 8 );
      head[3] = static_cast<char>( size );
      return head + aType + aBody;
   };
   const QByteArray audio = box( "stsd", "mp4a" );
   const QString av1 = fixture.path( "av1.mp4" );
   writeFile( av1, box( "ftyp", "isom" ) + box( "mdat", QByteArray( 100, 'x' ) ) +
                      box( "moov", box( "trak", audio ) + box( "trak", box( "stsd", "av01" ) ) ) );
   EXPECT_EQ( videoCodecName( av1 ), QStringLiteral( "AV1" ) );

   const QString avc = fixture.path( "avc.mp4" );
   writeFile( avc, box( "moov", box( "stsd", "avc1" ) ) );
   EXPECT_EQ( videoCodecName( avc ), QStringLiteral( "H.264" ) );

   const QString other = fixture.path( "other.mp4" );
   writeFile( other, "not a video" );
   EXPECT_TRUE( videoCodecName( other ).isEmpty() );
   EXPECT_TRUE( videoCodecName( fixture.path( "missing.mp4" ) ).isEmpty() );
}

}  // namespace
