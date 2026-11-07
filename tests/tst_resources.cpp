// Table integrity for Resources. A missing logo or a duplicate folder name is a
// missing picture or a wrong system at runtime, not a compile error.

#include "QtPrinters.h"
#include "Resources.h"

#include <gtest/gtest.h>

#include <QFile>
#include <QSet>

namespace {

// 84 systems, but fewer logo *files*: several kinds share one.
TEST( Resources, EnumCountsMatchTheTables )
{
   EXPECT_EQ( skCount, 84 );
   EXPECT_EQ( cnCount, 40 );
   EXPECT_EQ( lnCount, 5 );
}

TEST( Resources, EverySystemHasADisplayNameAndAScraperId )
{
   for ( int i = 0; i < skCount; ++i ) {
      SCOPED_TRACE( i );
      EXPECT_EQ( systemInfo( static_cast<SystemKind>( i ) ).kind, i );
      EXPECT_STRNE( systemInfo( static_cast<SystemKind>( i ) ).name, "" );
      EXPECT_STRNE( systemInfo( static_cast<SystemKind>( i ) ).ssId, "" );
   }
}

// A shifted row would send a system to another platform's scrape.
TEST( Resources, ScreenscraperIdsArePinnedForKnownSystems )
{
   EXPECT_STREQ( systemInfo( skNES ).ssId, "3" );
   EXPECT_STREQ( systemInfo( skSNES ).ssId, "4" );
   EXPECT_STREQ( systemInfo( skMasterSystem ).ssId, "2" );
   EXPECT_STREQ( systemInfo( skMegaDrive ).ssId, "1" );
   EXPECT_STREQ( systemInfo( skGB ).ssId, "9" );
   EXPECT_STREQ( systemInfo( skGBC ).ssId, "10" );
   EXPECT_STREQ( systemInfo( skGBA ).ssId, "12" );
   EXPECT_STREQ( systemInfo( skGG ).ssId, "21" );
}

// A table one row short leaves the tail null, a shifted row maps a system to the wrong platform.
TEST( Resources, LaunchBoxPlatformNamesLineUpWithTheKinds )
{
   for ( int i = 0; i < skCount; ++i ) {
      SCOPED_TRACE( i );
      EXPECT_NE( systemInfo( static_cast<SystemKind>( i ) ).lbName, nullptr );
   }

   EXPECT_STREQ( systemInfo( skNES ).lbName, "Nintendo Entertainment System" );
   EXPECT_STREQ( systemInfo( skMSX2 ).lbName, "Microsoft MSX2" );
   EXPECT_STREQ( systemInfo( skGW2 ).lbName, "Nintendo Game & Watch" );
   EXPECT_STREQ( systemInfo( skTI99 ).lbName, "Texas Instruments TI 99/4A" );
   EXPECT_STREQ( systemInfo( skPSPM ).lbName, "Sony PSP Minis" );
   EXPECT_STREQ( systemInfo( skWindows ).lbName, "Windows" );
   EXPECT_STREQ( systemInfo( skAndroid ).lbName, "Android" );
   EXPECT_STREQ( systemInfo( skCS ).lbName, "" );
   EXPECT_STREQ( systemInfo( skOther ).lbName, "" );
   EXPECT_STREQ( systemInfo( skMegaDrive ).lbName, systemInfo( skGenesis ).lbName );
}

// The logo is loaded with no existence check, so every one has to be in the .qrc.
TEST( Resources, EverySystemLogoResolvesInTheQrc )
{
   for ( int i = 0; i < skCount; ++i ) {
      const QString logo = Cst_LogoPicsFolder +
                           QLatin1String( systemInfo( static_cast<SystemKind>( i ) ).logo );
      EXPECT_TRUE( QFile::exists( logo ) ) << logo.toStdString();
   }
}

// A duplicate folder name would map to the wrong kind, and nothing downstream would
// notice.
TEST( Resources, FolderNamesAreUniqueAndRoundTrip )
{
   QSet<QString> seen;

   for ( int i = 0; i < skCount; ++i ) {
      const QString folder = QString::fromLatin1( systemInfo( static_cast<SystemKind>( i ) ).folder );
      SCOPED_TRACE( folder.toStdString() );

      EXPECT_FALSE( seen.contains( folder ) );
      seen.insert( folder );
      EXPECT_EQ( systemKindFromFolder( folder ), static_cast<SystemKind>( i ) );
   }
}

TEST( Resources, SystemKindFromFolderFallsBackToOther )
{
   EXPECT_EQ( systemKindFromFolder( QStringLiteral( "no-such-system" ) ), skOther );

   // Case-sensitive: real folders are lowercase.
   EXPECT_EQ( systemKindFromFolder( QStringLiteral( "NES" ) ), skOther );

   // skOther is the catch-all and its own folder name is empty.
   EXPECT_STREQ( systemInfo( skOther ).folder, "" );
}

TEST( Resources, CountryShortNamesRoundTrip )
{
   for ( int i = 0; i < cnCount; ++i ) {
      const QString shortName = QString::fromLatin1( Cst_CountryName[i] );
      SCOPED_TRACE( shortName.toStdString() );
      EXPECT_EQ( countryFromShortName( shortName ), static_cast<CountryName>( i ) );
   }

   EXPECT_EQ( countryFromShortName( QStringLiteral( "zz" ) ), cnUnd );

   // cnUnd's own short name is "", so an absent region already lands there.
   EXPECT_EQ( countryFromShortName( QString() ), cnUnd );
}

TEST( Resources, EveryCountryIsNamedInEveryLanguage )
{
   for ( int i = 0; i < cnCount; ++i ) {
      for ( int lang = 0; lang < lnCount; ++lang ) {
         SCOPED_TRACE( testing::Message() << i << '/' << lang );
         EXPECT_EQ( *Cst_CountryNameFull[i][lang] != '\0', i != cnUnd );
      }
   }
}

// The tables are const char*, so a source-encoding slip would show up as
// mojibake rather than as a compile error.
TEST( Resources, AccentedCountryNamesAreUtf8 )
{
   EXPECT_EQ( QString::fromUtf8( Cst_CountryNameFull[cnBr][lnFrench] ),
              QStringLiteral( "Br\u00e9sil" ) );
   EXPECT_EQ( QString::fromUtf8( Cst_CountryNameFull[cnDe][lnGerman] ),
              QStringLiteral( "Deutschland" ) );
   EXPECT_EQ( QString::fromUtf8( Cst_CountryNameFull[cnUk][lnGerman] ),
              QStringLiteral( "Gro\u00dfbritannien" ) );
}

// Out-of-range falling back to English is what makes Cst_IniLanguageDefault
// safe - see the note in Resources.h.
TEST( Resources, LangFromIndexFallsBackToEnglish )
{
   EXPECT_EQ( langFromIndex( lnFrench ), lnFrench );
   EXPECT_EQ( langFromIndex( lnGerman ), lnGerman );
   EXPECT_EQ( langFromIndex( -1 ), lnEnglish );
   EXPECT_EQ( langFromIndex( lnCount ), lnEnglish );
   EXPECT_EQ( langFromIndex( Cst_IniLanguageDefault ), lnEnglish );
}

}  // namespace
