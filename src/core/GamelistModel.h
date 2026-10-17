#pragma once

#include "Gamelist.h"
#include "GamelistFilter.h"

#include <QAbstractListModel>
#include <QSortFilterProxyModel>

// List model over a Gamelist: a row is an index into it and nothing else.
class GamelistModel : public QAbstractListModel
{
   Q_OBJECT

public:
   explicit GamelistModel( QObject* aParent = nullptr );

   // Borrowed, not owned: the caller keeps the Gamelist alive.
   // Null is legal and means "no system loaded", not an error.
   void setGamelist( const Gamelist* aGamelist );

   const FilterSpec& filter() const { return FFilter; }
   void setFilter( const FilterSpec& aFilter );

   bool accepts( int aRow ) const;

   // The games changed under the view: an edit can rename a row and can move it
   // out of the current filter, so this repaints.
   // It does not cover games being added or removed: rowCount() then changes
   // with no begin/endInsertRows, so call setGamelist() again to reset.
   void refresh();

   int rowCount( const QModelIndex& aParent = {} ) const override;
   QVariant data( const QModelIndex& aIndex, int aRole ) const override;

signals:
   void filterChanged();

private:
   const Gamelist* FGamelist = nullptr;
   FilterSpec FFilter;
};

class GamelistFilter : public QSortFilterProxyModel
{
   Q_OBJECT

public:
   explicit GamelistFilter( QObject* aParent = nullptr );

   void setSourceModel( QAbstractItemModel* aModel ) override;

protected:
   bool filterAcceptsRow( int aSourceRow, const QModelIndex& aParent ) const override;
   bool lessThan( const QModelIndex& aLeft, const QModelIndex& aRight ) const override;
};
