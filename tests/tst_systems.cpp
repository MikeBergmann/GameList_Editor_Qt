#include "QtPrinters.h"
#include "Systems.h"

#include <gtest/gtest.h>

#include <QSet>
#include <QSettings>

namespace {

// QSettings is shared by the whole test run, so each test leaves the map empty.
struct CleanMap : testing::Test
{
   void TearDown() override { QSettings().remove( QStringLiteral( "SystemMap" ) ); }
};

using SystemMap = CleanMap;

QVector<SystemEntry> unknownFolder( const QString& aFolder )
{
   return { { aFolder, skOther, QString() } };
}

TEST_F( SystemMap, AMappedFolderBecomesTheKnownSystemAndAnUnmappedOneStaysOther )
{
   setSystemMapping( QStringLiteral( "amiga500" ), skAM );

   QVector<SystemEntry> systems = unknownFolder( QStringLiteral( "amiga500" ) );
   systems.append( { QStringLiteral( "other" ), skOther, QString() } );
   applySystemMap( systems );

   EXPECT_EQ( systems[0].kind, skAM );
   EXPECT_EQ( systems[1].kind, skOther );
}

TEST_F( SystemMap, ASystemTheTablesKnowIsNeverRemapped )
{
   setSystemMapping( QStringLiteral( "snes" ), skAM );

   QVector<SystemEntry> systems{ { QStringLiteral( "snes" ), systemKindFromFolder( QStringLiteral( "snes" ) ),
                                   QString() } };
   const SystemKind before = systems[0].kind;
   applySystemMap( systems );

   EXPECT_EQ( systems[0].kind, before );
}

TEST_F( SystemMap, MappingToOtherForgetsTheMapping )
{
   setSystemMapping( QStringLiteral( "amiga500" ), skAM );
   setSystemMapping( QStringLiteral( "amiga500" ), skOther );

   QVector<SystemEntry> systems = unknownFolder( QStringLiteral( "amiga500" ) );
   applySystemMap( systems );

   EXPECT_EQ( systems[0].kind, skOther );
}

TEST( SystemKindChoices, OffersEverySystemOnceWithAUniqueLabelInOrder )
{
   const QVector<SystemKindChoice> choices = systemKindChoices();

   // Everything but the catch-all, which is "no mapping".
   ASSERT_EQ( choices.size(), static_cast<int>( skOther ) );

   QSet<QString> labels;
   QSet<int> kinds;
   for ( int i = 0; i < choices.size(); ++i ) {
      labels.insert( choices[i].label );
      kinds.insert( choices[i].kind );
      if ( i > 0 ) {
         EXPECT_LE( choices[i - 1].label.compare( choices[i].label, Qt::CaseInsensitive ), 0 );
      }
   }
   EXPECT_EQ( labels.size(), choices.size() );  // the table names repeat; the labels do not
   EXPECT_EQ( kinds.size(), choices.size() );
   EXPECT_FALSE( kinds.contains( skOther ) );
}

}  // namespace
