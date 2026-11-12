#include "GamesController.h"

#include "LibraryController.h"

GamesController::GamesController( LibraryController* aLibrary, QObject* aParent )
   : QObject( aParent ), FLibrary( aLibrary )
{
   for ( int category = 0; category < static_cast<int>( FilterCategory::Count ); ++category )
      FCategoryNames.append( QLatin1String( filterCategoryName( static_cast<FilterCategory>( category ) ) ) );

   FProxy.setSourceModel( &FModel );

   const auto countMoved = [this] { emit countChanged(); };
   connect( &FProxy, &QAbstractItemModel::rowsInserted, this, countMoved );
   connect( &FProxy, &QAbstractItemModel::rowsRemoved, this, countMoved );
   connect( &FProxy, &QAbstractItemModel::modelReset, this, countMoved );
   connect( &FProxy, &QAbstractItemModel::layoutChanged, this, countMoved );

   // The category stays across systems, the search and the selection do not.
   connect( FLibrary, &LibraryController::gamelistChanged, this, [this] {
      FFilter.search.clear();
      FFilter.reference = -1;
      FCurrent = -1;
      FModel.setGamelist( FLibrary->gamelist() );
      apply();
      emit currentChanged();
   } );
}

QString GamesController::countText() const
{
   const int shown = FProxy.rowCount();
   return FFilter.category == FilterCategory::All
      ? QStringLiteral( "%1 game(s) found." ).arg( shown )
      : QStringLiteral( "%1 / %2 game(s) found." ).arg( shown ).arg( FModel.rowCount() );
}

void GamesController::setCategory( int aCategory )
{
   if ( aCategory < 0 || aCategory >= static_cast<int>( FilterCategory::Count )
        || aCategory == category() )
      return;

   FFilter.category = static_cast<FilterCategory>( aCategory );
   apply();
}

void GamesController::setSearch( const QString& aSearch )
{
   if ( aSearch == FFilter.search )
      return;

   FFilter.search = aSearch;
   apply();
}

void GamesController::setListByRom( bool aListByRom )
{
   if ( aListByRom == FFilter.listByRom )
      return;

   FFilter.listByRom = aListByRom;
   apply();
}

void GamesController::setFullRomName( bool aFullRomName )
{
   if ( aFullRomName == FFilter.fullRomName )
      return;

   FFilter.fullRomName = aFullRomName;
   apply();
}

int GamesController::sourceRow( int aProxyRow ) const
{
   const QModelIndex source = FProxy.mapToSource( FProxy.index( aProxyRow, 0 ) );
   return source.isValid() ? source.row() : -1;
}

void GamesController::select( int aProxyRow )
{
   const int row = sourceRow( aProxyRow );
   if ( row == FCurrent )
      return;

   FCurrent = row;
   FFilter.reference = row;
   if ( filterDependsOnSelection( FFilter.category ) )
      apply();

   emit currentChanged();
}

int GamesController::currentRow() const
{
   if ( FCurrent < 0 )
      return -1;
   const QModelIndex proxy = FProxy.mapFromSource( FModel.index( FCurrent, 0 ) );
   return proxy.isValid() ? proxy.row() : -1;
}

bool GamesController::step( int aDelta )
{
   const int row = currentRow();
   const int target = row + aDelta;
   if ( row < 0 || target < 0 || target >= FProxy.rowCount() )
      return false;

   select( target );
   return true;
}

void GamesController::apply()
{
   FModel.setFilter( FFilter );
   emit filterChanged();
   emit countChanged();
}
