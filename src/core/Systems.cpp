#include "Systems.h"

#include "PathUtil.h"

#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>

#include <algorithm>

namespace {

// %ESPATH% is the folder of the ES-DE program, which a file editor cannot know; the app-data
// folder's parent is where portable and Android installs keep both.
QString expandEsdePath( QString aPath, const QString& aAppData )
{
   aPath.replace( QLatin1String( "%ESPATH%" ), QDir::cleanPath( aAppData + QLatin1String( "/.." ) ) );
   if ( aPath == QLatin1String( "~" ) || aPath.startsWith( QLatin1String( "~/" ) ) )
      aPath.replace( 0, 1, QDir::homePath() );
   return QDir::cleanPath( aPath );
}

// es_settings.xml is a flat list of <string name="…" value="…"/>, with no root element.
QString esdeSetting( const QString& aAppData, const char* aName )
{
   QFile file( aAppData + QLatin1String( "/settings/es_settings.xml" ) );
   QDomDocument settings;
   if ( !file.open( QIODevice::ReadOnly ) || !settings.setContent( wrapInRoot( file.readAll(), "settings" ) ) )
      return {};

   for ( QDomElement node = settings.documentElement().firstChildElement( QStringLiteral( "string" ) );
         !node.isNull(); node = node.nextSiblingElement( QStringLiteral( "string" ) ) ) {
      if ( node.attribute( QStringLiteral( "name" ) ) == QLatin1String( aName ) )
         return node.attribute( QStringLiteral( "value" ) );
   }
   return {};
}

QString esdeRomRoot( const QString& aAppData )
{
   // Desktop, portable and Android installs all keep ROMs beside the app-data folder.
   QStringList candidates{ QDir::cleanPath( aAppData + QLatin1String( "/../ROMs" ) ) };

   if ( const QString value = esdeSetting( aAppData, "ROMDirectory" ); !value.isEmpty() )
      candidates.prepend( expandEsdePath( value, aAppData ) );

   // A card moved to another device keeps the old absolute path in the settings.
   for ( const QString& candidate : std::as_const( candidates ) )
      if ( QDir( candidate ).exists() )
         return candidate;
   return candidates.first();
}

QString esdeMediaRoot( const QString& aAppData )
{
   const QString value = esdeSetting( aAppData, "MediaDirectory" );
   return value.isEmpty() ? QDir::cleanPath( aAppData + QLatin1String( "/downloaded_media" ) )
                          : expandEsdePath( value, aAppData );
}

}  // namespace

QVector<SystemEntry> scanSystems( const QString& aRootPath )
{
   QVector<SystemEntry> systems;

   // An ES-DE app-data folder keeps its gamelists apart from the ROMs.
   const QDir root( aRootPath );
   const bool esde = root.exists( QStringLiteral( "settings/es_settings.xml" ) ) &&
                     root.exists( QStringLiteral( "gamelists" ) );
   const QDir lists( esde ? root.filePath( QStringLiteral( "gamelists" ) ) : aRootPath );
   const QString romRoot = esde ? esdeRomRoot( aRootPath ) : QString();
   const QString mediaRoot = esde ? esdeMediaRoot( aRootPath ) : QString();

   // Folders by attribute, not by a '*.*' name filter, which needs a literal dot and
   // so would miss folder names like "snes". Hidden folders are left out.
   const QStringList folders = lists.entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name );

   for ( const QString& folder : folders ) {
      const QString gamelist = resolvePath(
         lists.filePath( folder + QLatin1Char( '/' ) + QLatin1String( Cst_GameListFileName ) ) );
      if ( !QFileInfo::exists( gamelist ) )
         continue;

      systems.append( { folder, systemKindFromFolder( folder ), gamelist,
                        esde ? romRoot + QLatin1Char( '/' ) + folder : QString(),
                        esde ? mediaRoot + QLatin1Char( '/' ) + folder : QString() } );
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
