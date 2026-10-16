// Table integrity for Resources. A missing logo or a duplicate folder name is a
// missing picture or a wrong system at runtime, not a compile error.

#include "QtPrinters.h"
#include "Resources.h"

#include <gtest/gtest.h>

#include <QFile>
#include <QSet>

namespace {

// 82 systems, not 76: 76 is the count of unique logo *files*, and six kinds share one.
TEST( Resources, EnumCountsMatchTheTables )
{
   EXPECT_EQ( skCount, 82 );
   EXPECT_EQ( cnCount, 40 );
   EXPECT_EQ( lnCount, 5 );
}

TEST( Resources, EverySystemHasADisplayNameAndAScraperId )
{
   for ( int i = 0; i < skCount; ++i ) {
      SCOPED_TRACE( i );
      EXPECT_STRNE( Cst_SystemKindStr[i], "" );
      EXPECT_STRNE( Cst_SystemKindId[i], "" );
   }
}

// The logo is loaded with no existence check, so every one has to be in the .qrc.
TEST( Resources, EverySystemLogoResolvesInTheQrc )
{
   for ( int i = 0; i < skCount; ++i ) {
      const QString logo =
         QLatin1String( Cst_LogoPicsFolder ) + QLatin1String( Cst_SystemKindImageNames[i] );
      EXPECT_TRUE( QFile::exists( logo ) ) << logo.toStdString();
   }
}

// A duplicate folder name would map to the wrong kind, and nothing downstream would
// notice.
TEST( Resources, FolderNamesAreUniqueAndRoundTrip )
{
   QSet<QString> seen;

   for ( int i = 0; i < skCount; ++i ) {
      const QString folder = QString::fromLatin1( Cst_SystemKindFolderNames[i] );
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
   EXPECT_STREQ( Cst_SystemKindFolderNames[skOther], "" );
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
