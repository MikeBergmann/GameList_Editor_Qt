#include "GamelistModel.h"

GamelistModel::GamelistModel( QObject* aParent ) : QAbstractListModel( aParent ) {}

void GamelistModel::setGamelist( const Gamelist* aGamelist )
{
   beginResetModel();
   FGamelist = aGamelist;
   endResetModel();
}

void GamelistModel::setFilter( const FilterSpec& aFilter )
{
   // Chk_ListByRom and Chk_FullRomName change what every row reads, not which
   // rows there are - so they are a repaint, and the selection survives them.
   const bool displayChanged =
      aFilter.listByRom != FFilter.listByRom || aFilter.fullRomName != FFilter.fullRomName;

   FFilter = aFilter;

   if ( displayChanged && rowCount() > 0 )
      emit dataChanged( index( 0 ), index( rowCount() - 1 ), { Qt::DisplayRole } );

   emit filterChanged();
}

bool GamelistModel::accepts( int aRow ) const
{
   return FGamelist && FGamelist->accepts( aRow, FFilter );
}

void GamelistModel::refresh()
{
   if ( rowCount() > 0 )
      emit dataChanged( index( 0 ), index( rowCount() - 1 ) );

   emit filterChanged();
}

int GamelistModel::rowCount( const QModelIndex& aParent ) const
{
   // A list model has rows only at the root.
   if ( aParent.isValid() || !FGamelist )
      return 0;

   return FGamelist->count();
}

QVariant GamelistModel::data( const QModelIndex& aIndex, int aRole ) const
{
   if ( !FGamelist || !aIndex.isValid() || aIndex.row() >= FGamelist->count() )
      return {};

   if ( aRole == Qt::DisplayRole )
      return FGamelist->displayName( aIndex.row(), FFilter );

   if ( aRole == Qt::ToolTipRole )
      return FGamelist->at( aIndex.row() ).romPath;

   return {};
}

GamelistFilter::GamelistFilter( QObject* aParent ) : QSortFilterProxyModel( aParent )
{
   // Sorted by what the row displays, so it follows Chk_ListByRom / Chk_FullRomName
   // and edits (dataChanged re-sorts while dynamicSortFilter is on, the default).
   setSortCaseSensitivity( Qt::CaseInsensitive );

   sort( 0 );
}

void GamelistFilter::setSourceModel( QAbstractItemModel* aModel )
{
   if ( auto* previous = qobject_cast<GamelistModel*>( sourceModel() ) )
      disconnect( previous, &GamelistModel::filterChanged, this, nullptr );

   QSortFilterProxyModel::setSourceModel( aModel );

   if ( auto* source = qobject_cast<GamelistModel*>( aModel ) ) {
      connect( source, &GamelistModel::filterChanged, this, [this] { invalidateRowsFilter(); } );
   }
}

bool GamelistFilter::filterAcceptsRow( int aSourceRow, const QModelIndex& aParent ) const
{
   if ( aParent.isValid() )
      return false;

   auto* source = qobject_cast<GamelistModel*>( sourceModel() );
   return source && source->accepts( aSourceRow );
}

bool GamelistFilter::lessThan( const QModelIndex& aLeft, const QModelIndex& aRight ) const
{
   // Equal names (duplicates are common) keep gamelist order, so the list does
   // not reshuffle itself between refreshes.
   const int order = QString::compare( aLeft.data().toString(), aRight.data().toString(),
                                       sortCaseSensitivity() );
   return order != 0 ? order < 0 : aLeft.row() < aRight.row();
}
