// GamelistModel and its proxy: the rows a view sees are the games the filter
// accepts, and a row maps straight back to an index into the Gamelist.

#include "GamelistFixture.h"
#include "GamelistModel.h"
#include "QtPrinters.h"

#include <gtest/gtest.h>

namespace {

struct ModelFixture : Fixture
{
   Gamelist list;
   GamelistModel model;
   GamelistFilter proxy;

   ModelFixture()
   {
      EXPECT_TRUE( list.load( snes ) );
      model.setGamelist( &list );
      proxy.setSourceModel( &model );
   }

   void setFilter( const FilterSpec& aFilter ) { model.setFilter( aFilter ); }

   QStringList shown() const
   {
      QStringList rows;
      for ( int row = 0; row < proxy.rowCount(); ++row )
         rows.append( proxy.index( row, 0 ).data().toString() );
      return rows;
   }
};

TEST( GamelistModel, ShowsEveryGameUntilAFilterSaysOtherwise )
{
   ModelFixture fixture;

   EXPECT_EQ( fixture.model.rowCount(), 3 );
   EXPECT_EQ( fixture.shown(), QStringList( { QStringLiteral( "Sonic The Hedgehog" ),
                                               QStringLiteral( "Sonic The Hedgehog" ),
                                               QStringLiteral( "Vanished" ) } ) );
}

TEST( GamelistModel, IsEmptyWithoutAGamelist )
{
   GamelistModel model;
   GamelistFilter proxy;
   proxy.setSourceModel( &model );

   // No system chosen yet is the startup state, not an error.
   EXPECT_EQ( model.rowCount(), 0 );
   EXPECT_EQ( proxy.rowCount(), 0 );
   EXPECT_FALSE( model.accepts( 0 ) );
   EXPECT_FALSE( model.index( 0, 0 ).data().isValid() );
}

// Setting the filter has to reach the proxy on its own: the Delphi rebuilt the
// listbox by hand from four separate handlers instead.
TEST( GamelistModel, ReFiltersWhenTheSpecChanges )
{
   ModelFixture fixture;

   FilterSpec filter;
   filter.index = 14;  // orphan
   fixture.setFilter( filter );
   EXPECT_EQ( fixture.shown(), QStringList( { QStringLiteral( "Vanished" ) } ) );

   filter.index = 13;  // favorite
   fixture.setFilter( filter );
   EXPECT_EQ( fixture.shown(), QStringList( { QStringLiteral( "Sonic The Hedgehog" ) } ) );

   filter.index = 0;
   filter.search = QStringLiteral( "vanish" );
   fixture.setFilter( filter );
   EXPECT_EQ( fixture.shown(), QStringList( { QStringLiteral( "Vanished" ) } ) );
}

TEST( GamelistModel, DisplayFollowsTheListByRomCheckboxes )
{
   ModelFixture fixture;

   FilterSpec filter;
   filter.listByRom = true;
   fixture.setFilter( filter );
   EXPECT_EQ( fixture.shown(), QStringList( { QStringLiteral( "Sonic.zip" ),
                                               QStringLiteral( "Sonic.zip" ),
                                               QStringLiteral( "Vanished.zip" ) } ) );

   filter.fullRomName = true;
   fixture.setFilter( filter );
   EXPECT_EQ( fixture.shown(), QStringList( { QStringLiteral( "Sonic.zip" ),
                                               QStringLiteral( "sub/Sonic.zip" ),
                                               QStringLiteral( "Vanished.zip" ) } ) );
}

// The two rows that read "Sonic The Hedgehog" are different games, and this is
// the only thing on screen that says which is which.
TEST( GamelistModel, TooltipIsTheRomPath )
{
   ModelFixture fixture;

   EXPECT_EQ( fixture.proxy.index( 1, 0 ).data( Qt::ToolTipRole ).toString(),
              QStringLiteral( "./sub/Sonic.zip" ) );
}

TEST( GamelistModel, ProxyRowsMapBackToGamelistIndices )
{
   ModelFixture fixture;

   FilterSpec filter;
   filter.index = 29;  // duplicate names: drops "Vanished"
   fixture.setFilter( filter );
   ASSERT_EQ( fixture.proxy.rowCount(), 2 );

   // Pick the second row, then filter by "same folder as that one".
   const int reference = fixture.proxy.mapToSource( fixture.proxy.index( 1, 0 ) ).row();
   EXPECT_EQ( reference, 1 );
   EXPECT_EQ( fixture.list.at( reference ).romPath, QStringLiteral( "./sub/Sonic.zip" ) );

   filter.index = 26;  // same folder
   filter.reference = reference;
   fixture.setFilter( filter );

   ASSERT_EQ( fixture.proxy.rowCount(), 1 );
   EXPECT_EQ( fixture.proxy.mapToSource( fixture.proxy.index( 0, 0 ) ).row(), reference );
}

}  // namespace
