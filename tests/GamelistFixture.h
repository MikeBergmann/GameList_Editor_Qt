#pragma once

// A ROM root on disk, shared by the Gamelist and GamelistModel tests: two system
// folders with the same gamelist, some media present and some not, and the case
// mismatches

#include "Gamelist.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

inline void writeFile( const QString& aPath, const QByteArray& aContent )
{
   ASSERT_TRUE( QDir().mkpath( QFileInfo( aPath ).path() ) );
   QFile file( aPath );
   ASSERT_TRUE( file.open( QIODevice::WriteOnly ) );
   ASSERT_EQ( file.write( aContent ), aContent.size() );
}

// Four games, in the shape EmulationStation writes: paths relative to the system
// folder, dates as yyyymmddT000000, flags as the literal "true".
inline const QByteArray Cst_Gamelist = R"(<?xml version="1.0"?>
<gameList>
   <game>
      <path>./Sonic.zip</path>
      <name>Sonic The Hedgehog</name>
      <desc>Gotta go fast</desc>
      <image>./media/images/Sonic.png</image>
      <video>./media/videos/Sonic.mp4</video>
      <rating>0,8</rating>
      <releasedate>19910623T000000</releasedate>
      <developer>Sonic Team</developer>
      <publisher>Sega</publisher>
      <genre>Platform</genre>
      <players>1</players>
      <region>wor</region>
      <favorite>true</favorite>
   </game>
   <game>
      <path>./sub/Sonic.zip</path>
      <name>Sonic The Hedgehog</name>
      <image>./media/images/absent.png</image>
      <releasedate>19910000T000000</releasedate>
      <rating>0.5</rating>
      <kidgame>true</kidgame>
      <hidden>False</hidden>
   </game>
   <game>
      <path>./Vanished.zip</path>
      <name>Vanished</name>
      <publisher>Sega</publisher>
   </game>
   <game/>
</gameList>
)";

// Neither system folder has a dot in its name, which is what trap 1 is about.
struct Fixture
{
   QTemporaryDir dir;
   SystemEntry snes;

   Fixture()
   {
      writeFile( path( "snes/gamelist.xml" ), Cst_Gamelist );
      writeFile( path( "snes/Sonic.zip" ), "rom" );
      writeFile( path( "snes/sub/SONIC.ZIP" ), "rom" );  // same name, other case
      writeFile( path( "snes/media/images/sonic.PNG" ), "png" );
      writeFile( path( "snes/media/videos/Sonic.mp4" ), "mp4" );
      writeFile( path( "megadrive/gamelist.xml" ), Cst_Gamelist );
      EXPECT_TRUE( QDir().mkpath( path( "no-gamelist-here" ) ) );

      snes = { QStringLiteral( "snes" ), skSNES, path( "snes/gamelist.xml" ) };
   }

   QString path( const char* aRelative ) const
   {
      return dir.filePath( QLatin1String( aRelative ) );
   }
};
