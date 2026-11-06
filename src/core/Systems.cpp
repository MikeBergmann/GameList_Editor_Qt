#include "Systems.h"

#include "PathUtil.h"

#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
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

// An ES-DE app-data folder keeps its gamelists apart from the ROMs.
struct RootLayout
{
   bool esde = false;
   QDir lists;
   QString romRoot;
   QString mediaRoot;
};

RootLayout rootLayout( const QString& aRootPath )
{
   const QDir root( aRootPath );
   RootLayout layout;
   layout.esde = root.exists( QStringLiteral( "settings/es_settings.xml" ) ) &&
                 root.exists( QStringLiteral( "gamelists" ) );
   layout.lists = QDir( layout.esde ? root.filePath( QStringLiteral( "gamelists" ) ) : aRootPath );
   if ( layout.esde ) {
      layout.romRoot = esdeRomRoot( aRootPath );
      layout.mediaRoot = esdeMediaRoot( aRootPath );
   }
   return layout;
}

SystemEntry entryIn( const RootLayout& aLayout, const QString& aFolder, const QString& aGamelistPath )
{
   return { aFolder, systemKindFromFolder( aFolder ), aGamelistPath,
            aLayout.esde ? aLayout.romRoot + QLatin1Char( '/' ) + aFolder : QString(),
            aLayout.esde ? aLayout.mediaRoot + QLatin1Char( '/' ) + aFolder : QString() };
}

}  // namespace

QVector<SystemEntry> scanSystems( const QString& aRootPath )
{
   QVector<SystemEntry> systems;
   const RootLayout layout = rootLayout( aRootPath );

   // Folders by attribute, not by a '*.*' name filter, which needs a literal dot and
   // so would miss folder names like "snes". Hidden folders are left out.
   const QStringList folders = layout.lists.entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name );

   for ( const QString& folder : folders ) {
      const QString gamelist = resolvePath(
         layout.lists.filePath( folder + QLatin1Char( '/' ) + QLatin1String( Cst_GameListFileName ) ) );
      if ( !QFileInfo::exists( gamelist ) )
         continue;

      systems.append( entryIn( layout, folder, gamelist ) );
   }

   return systems;
}

SystemEntry systemAt( const QString& aRootPath, const QString& aFolderName )
{
   const RootLayout layout = rootLayout( aRootPath );
   const QString folder = resolvePath( layout.lists.filePath( aFolderName ) );
   return entryIn( layout, aFolderName,
                   folder + QLatin1Char( '/' ) + QLatin1String( Cst_GameListFileName ) );
}

Result<> createGamelist( const QString& aPath )
{
   if ( QFileInfo::exists( aPath ) )
      return Error{ QStringLiteral( "%1 already exists." ).arg( aPath ) };

   const QString folder = QFileInfo( aPath ).path();
   if ( !QDir().mkpath( folder ) )
      return Error{ QStringLiteral( "Cannot create %1." ).arg( folder ) };

   QSaveFile file( aPath );
   if ( !file.open( QIODevice::WriteOnly ) )
      return Error{ QStringLiteral( "Cannot write %1: %2" ).arg( aPath, file.errorString() ) };

   file.write( "<?xml version=\"1.0\"?>\n<gameList/>\n" );
   if ( !file.commit() )
      return Error{ QStringLiteral( "Cannot write %1: %2" ).arg( aPath, file.errorString() ) };
   return {};
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
