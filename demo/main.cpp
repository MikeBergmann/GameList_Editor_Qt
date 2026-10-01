// Throwaway proof that GamelistModel works on real data: no editing, no saving.
// Usage: gamelistmodel_demo /path/to/roms   (the folder that holds snes/, nes/, ...)

#include "GamelistModel.h"

#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QVBoxLayout>

int main( int argc, char* argv[] )
{
   QApplication app( argc, argv );

   if ( argc < 2 ) {
      qCritical( "usage: %s /path/to/roms", argv[0] );
      return 2;
   }

   const QVector<SystemEntry> systems = scanSystems( QString::fromLocal8Bit( argv[1] ) );
   if ( systems.isEmpty() ) {
      qCritical( "no system folder with a gamelist.xml under %s", argv[1] );
      return 1;
   }

   Gamelist list;
   GamelistModel model;
   GamelistFilter proxy;
   proxy.setSourceModel( &model );

   QWidget window;
   auto* layout = new QVBoxLayout( &window );
   auto* Cbx_Systems = new QComboBox;
   auto* Edt_Search = new QLineEdit;
   auto* Lbx_Games = new QListView;
   auto* Lbl_Status = new QLabel;
   Edt_Search->setPlaceholderText( "search" );
   Lbx_Games->setModel( &proxy );
   for ( const SystemEntry& system : systems )
      Cbx_Systems->addItem( system.folderName, QVariant::fromValue( system ) );
   layout->addWidget( Cbx_Systems );
   layout->addWidget( Edt_Search );
   layout->addWidget( Lbx_Games );
   layout->addWidget( Lbl_Status );

   auto updateStatus = [&]( const QString& aError = {} ) {
      Lbl_Status->setText(
         aError.isEmpty()
            ? QString( "%1 / %2 games" ).arg( proxy.rowCount() ).arg( model.rowCount() )
            : aError );
   };

   QObject::connect( Cbx_Systems, &QComboBox::currentIndexChanged, [&] {
      QString error;
      model.setGamelist( nullptr );
      if ( list.load( Cbx_Systems->currentData().value<SystemEntry>(), &error ) )
         model.setGamelist( &list );
      updateStatus( error );
   } );

   QObject::connect( Edt_Search, &QLineEdit::textChanged, [&]( const QString& aText ) {
      FilterSpec filter = model.filter();
      filter.search = aText;
      model.setFilter( filter );
      updateStatus();
   } );

   emit Cbx_Systems->currentIndexChanged( 0 );
   window.resize( 480, 640 );
   window.show();
   return app.exec();
}
