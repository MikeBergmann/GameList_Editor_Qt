#include "LibraryController.h"

#include <QDir>
#include <QSettings>
#include <QVariantMap>

LibraryController::LibraryController( QObject* aParent ) : QObject( aParent ) {}

QString LibraryController::lastFolder() const
{
   QSettings settings;
   settings.beginGroup( Cst_IniOptions );
   const QString last = settings.value( Cst_IniLastFolder ).toString();
   return QDir( last ).exists() ? last : QString();
}

void LibraryController::rebuildItems()
{
   QSettings settings;
   settings.beginGroup( Cst_IniOptions );
   const bool genesis = settings.value( Cst_IniGenesisLogo, false ).toBool();

   FSystemItems.clear();
   for ( const SystemEntry& system : std::as_const( FSystems ) ) {
      const SystemKind logoKind = ( system.kind == skMegaDrive && genesis ) ? skGenesis : system.kind;
      FSystemItems.append( QVariantMap{
         { QStringLiteral( "name" ), systemDisplayName( system, genesis ) },
         { QStringLiteral( "logo" ), QStringLiteral( "qrc" ) + Cst_LogoPicsFolder + QLatin1String( systemInfo( logoKind ).logo ) },
         { QStringLiteral( "folder" ), system.folderName } } );
   }
}

QStringList LibraryController::kindLabels() const
{
   QStringList labels{ QStringLiteral( "Other (no mapping)" ) };
   for ( const SystemKindChoice& choice : FChoices )
      labels.append( choice.label );
   return labels;
}

bool LibraryController::canSetKind( int aSystem ) const
{
   return aSystem >= 0 && aSystem < FSystems.size()
          && systemKindFromFolder( FSystems.at( aSystem ).folderName ) == skOther;
}

int LibraryController::kindChoice( int aSystem ) const
{
   if ( aSystem < 0 || aSystem >= FSystems.size() )
      return 0;

   for ( int i = 0; i < FChoices.size(); ++i )
      if ( FChoices.at( i ).kind == FSystems.at( aSystem ).kind )
         return i + 1;
   return 0;
}

void LibraryController::setKind( int aSystem, int aChoice )
{
   if ( !canSetKind( aSystem ) || aChoice < 0 || aChoice > FChoices.size() )
      return;

   const SystemKind kind = aChoice == 0 ? skOther : FChoices.at( aChoice - 1 ).kind;
   setSystemMapping( FSystems.at( aSystem ).folderName, kind );
   FSystems[aSystem].kind = kind;
   rebuildItems();
   emit systemsChanged();
}

bool LibraryController::openRoot( const QString& aRootPath )
{
   FLoaded = false;
   FCurrent = -1;
   FError.clear();
   FRootPath = aRootPath;
   FSystems = scanSystems( aRootPath );
   applySystemMap( FSystems );
   rebuildItems();

   if ( FSystems.isEmpty() ) {
      FError = QStringLiteral( "No gamelist.xml found under %1." ).arg( aRootPath );
   } else {
      QSettings settings;
      settings.beginGroup( Cst_IniOptions );
      settings.setValue( Cst_IniLastFolder, aRootPath );
   }

   emit systemsChanged();
   emit gamelistChanged();
   return FError.isEmpty();
}

bool LibraryController::selectSystem( int aIndex )
{
   FLoaded = false;
   FCurrent = -1;
   FError.clear();
   emit gamelistChanged();

   if ( aIndex < 0 || aIndex >= FSystems.size() ) {
      FError = QStringLiteral( "No such system." );
   } else if ( const Result<> loaded = FGamelist.load( FSystems.at( aIndex ) ); !loaded ) {
      FError = loaded.error();
   } else {
      FLoaded = true;
      FCurrent = aIndex;
   }

   emit gamelistChanged();
   return FLoaded;
}
