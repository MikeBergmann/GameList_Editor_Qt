#pragma once

#include "GamelistModel.h"

#include <QObject>
#include <QStringList>

class LibraryController;

// The game list of the loaded system: the filtered model plus the filter state. A row is a
// proxy row; sourceRow() turns it into the index every other call uses.
class GamesController : public QObject
{
   Q_OBJECT
   Q_PROPERTY( QObject* model READ model CONSTANT )
   Q_PROPERTY( QStringList categoryNames READ categoryNames CONSTANT )
   Q_PROPERTY( int category READ category WRITE setCategory NOTIFY filterChanged )
   Q_PROPERTY( QString search READ search WRITE setSearch NOTIFY filterChanged )
   Q_PROPERTY( bool listByRom READ listByRom WRITE setListByRom NOTIFY filterChanged )
   Q_PROPERTY( bool fullRomName READ fullRomName WRITE setFullRomName NOTIFY filterChanged )
   Q_PROPERTY( QString countText READ countText NOTIFY countChanged )
   Q_PROPERTY( int current READ current NOTIFY currentChanged )

public:
   explicit GamesController( LibraryController* aLibrary, QObject* aParent = nullptr );

   QObject* model() { return &FProxy; }
   const QStringList& categoryNames() const { return FCategoryNames; }
   int category() const { return static_cast<int>( FFilter.category ); }
   const QString& search() const { return FFilter.search; }
   bool listByRom() const { return FFilter.listByRom; }
   bool fullRomName() const { return FFilter.fullRomName; }
   QString countText() const;
   // Index of the selected game in the Gamelist, -1 for none.
   int current() const { return FCurrent; }

   void setCategory( int aCategory );
   void setSearch( const QString& aSearch );
   void setListByRom( bool aListByRom );
   void setFullRomName( bool aFullRomName );

   Q_INVOKABLE int sourceRow( int aProxyRow ) const;
   // Makes the row the selected game; the "same as" categories follow it.
   Q_INVOKABLE void select( int aProxyRow );
   // The games changed under the list (an edit can rename a row or move it out of the filter).
   void refresh() { FModel.refresh(); }

signals:
   void filterChanged();
   void countChanged();
   void currentChanged();

private:
   void apply();

   LibraryController* FLibrary;
   GamelistModel FModel;
   GamelistFilter FProxy;
   FilterSpec FFilter;
   QStringList FCategoryNames;
   int FCurrent = -1;
};
