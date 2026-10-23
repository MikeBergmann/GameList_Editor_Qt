#include "Systems.h"

#include "PathUtil.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>

#include <algorithm>

QVector<SystemEntry> scanSystems( const QString& aRootPath )
{
   QVector<SystemEntry> systems;

   // Folders by attribute, not by a '*.*' name filter, which needs a literal dot and
   // so would miss folder names like "snes". Hidden folders are left out.
   const QDir root( aRootPath );
   const QStringList folders = root.entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name );

   for ( const QString& folder : folders ) {
      const QString gamelist = resolvePath(
         root.filePath( folder + QLatin1Char( '/' ) + QLatin1String( Cst_GameListFileName ) ) );
      if ( !QFileInfo::exists( gamelist ) )
         continue;

      systems.append( { folder, systemKindFromFolder( folder ), gamelist } );
   }

   return systems;
}

void applySystemMap( QVector<SystemEntry>& aSystems )
{
   QSettings settings;
   settings.beginGroup( QLatin1String( Cst_IniSystemMap ) );
   for ( SystemEntry& system : aSystems )
      if ( system.kind == skOther && settings.contains( system.folderName ) )
         system.kind = systemKindFromFolder( settings.value( system.folderName ).toString() );
}

void setSystemMapping( const QString& aFolderName, SystemKind aKind )
{
   QSettings settings;
   settings.beginGroup( QLatin1String( Cst_IniSystemMap ) );
   if ( aKind == skOther )
      settings.remove( aFolderName );
   else
      settings.setValue( aFolderName, QLatin1String( Cst_SystemKindFolderNames[aKind] ) );
}

QVector<SystemKindChoice> systemKindChoices()
{
   QVector<SystemKindChoice> choices;
   for ( int kind = 0; kind < skOther; ++kind )
      choices.append( { QStringLiteral( "%1 (%2)" )
                           .arg( QLatin1String( Cst_SystemKindStr[kind] ),
                                 QLatin1String( Cst_SystemKindFolderNames[kind] ) ),
                        static_cast<SystemKind>( kind ) } );

   std::sort( choices.begin(), choices.end(), []( const SystemKindChoice& a, const SystemKindChoice& b ) {
      return a.label.compare( b.label, Qt::CaseInsensitive ) < 0;
   } );
   return choices;
}

QString systemDisplayName( const SystemEntry& aSystem, bool aGenesis )
{
   // skOther is every folder no table knows, so its own name is all there is.
   if ( aSystem.kind == skOther )
      return aSystem.folderName;

   QString name = QLatin1String( Cst_SystemKindStr[aSystem.kind] );
   if ( aSystem.kind == skMegaDrive && aGenesis )
      name = QLatin1String( Cst_SystemKindStr[skGenesis] );

   // A mapped folder ("amiga500" treated as Amiga) keeps its own name visible.
   if ( aSystem.folderName != QLatin1String( Cst_SystemKindFolderNames[aSystem.kind] ) )
      name += QStringLiteral( " (%1)" ).arg( aSystem.folderName );
   return name;
}
