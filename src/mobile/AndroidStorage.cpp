#include "AndroidStorage.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QMimeDatabase>

AndroidStorage::AndroidStorage( QObject* aParent ) : QObject( aParent )
{
   refresh();
   connect( qGuiApp, &QGuiApplication::applicationStateChanged, this, [this]( Qt::ApplicationState aState ) {
      if ( aState == Qt::ApplicationActive )
         refresh();
   } );
}

void AndroidStorage::refresh()
{
   const bool granted =
      QJniObject::callStaticMethod<jboolean>( "android/os/Environment", "isExternalStorageManager" );
   if ( granted == FHasAccess )
      return;

   FHasAccess = granted;
   emit hasAccessChanged();
}

QStringList AndroidStorage::externalVolumes()
{
   QStringList result;
   const QJniObject context = QNativeInterface::QAndroidApplication::context();
   const QJniObject manager = context.callObjectMethod(
      "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;",
      QJniObject::fromString( QStringLiteral( "storage" ) ).object<jstring>() );
   const QJniObject volumes = manager.callObjectMethod( "getStorageVolumes", "()Ljava/util/List;" );
   const jint count = volumes.isValid() ? volumes.callMethod<jint>( "size" ) : 0;
   for ( jint i = 0; i < count; ++i ) {
      const QJniObject volume = volumes.callObjectMethod( "get", "(I)Ljava/lang/Object;", i );
      if ( volume.callMethod<jboolean>( "isPrimary" ) )
         continue;
      // Limitation: StorageVolume.getDirectory needs API 30; older devices get no extra volumes.
      const QJniObject dir = volume.callObjectMethod( "getDirectory", "()Ljava/io/File;" );
      if ( dir.isValid() )
         result.append( dir.callObjectMethod<jstring>( "getAbsolutePath" ).toString() );
   }

   QJniEnvironment().checkAndClearExceptions();
   return result;
}

void AndroidStorage::request()
{
   const QJniObject context = QNativeInterface::QAndroidApplication::context();
   const QString package = context.callObjectMethod<jstring>( "getPackageName" ).toString();
   const QJniObject uri = QJniObject::callStaticObjectMethod(
      "android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;",
      QJniObject::fromString( QStringLiteral( "package:" ) + package ).object<jstring>() );
   const QJniObject action = QJniObject::getStaticObjectField<jstring>(
      "android/provider/Settings", "ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION" );
   const QJniObject intent( "android/content/Intent", "(Ljava/lang/String;Landroid/net/Uri;)V",
                            action.object<jstring>(), uri.object() );
   // FLAG_ACTIVITY_NEW_TASK: needed when the context is not an activity.
   intent.callObjectMethod( "addFlags", "(I)Landroid/content/Intent;", 0x10000000 );
   context.callMethod<void>( "startActivity", "(Landroid/content/Intent;)V", intent.object() );

   // Limitation: no fallback to the generic all-files page on a device without the per-app one.
   QJniEnvironment().checkAndClearExceptions();
}

bool AndroidStorage::openFile( const QString& aPath )
{
   QJniEnvironment env;
   const QJniObject context = QNativeInterface::QAndroidApplication::context();
   const QString authority =
      context.callObjectMethod<jstring>( "getPackageName" ).toString() + QStringLiteral( ".fileprovider" );
   const QJniObject file( "java/io/File", "(Ljava/lang/String;)V", QJniObject::fromString( aPath ).object<jstring>() );
   const QJniObject uri = QJniObject::callStaticObjectMethod(
      "androidx/core/content/FileProvider", "getUriForFile",
      "(Landroid/content/Context;Ljava/lang/String;Ljava/io/File;)Landroid/net/Uri;", context.object(),
      QJniObject::fromString( authority ).object<jstring>(), file.object() );
   if ( env.checkAndClearExceptions() || !uri.isValid() )
      return false;

   const QString mime = QMimeDatabase().mimeTypeForFile( aPath, QMimeDatabase::MatchExtension ).name();
   const QJniObject intent( "android/content/Intent", "(Ljava/lang/String;)V",
                            QJniObject::fromString( QStringLiteral( "android.intent.action.VIEW" ) ).object<jstring>() );
   intent.callObjectMethod( "setDataAndType", "(Landroid/net/Uri;Ljava/lang/String;)Landroid/content/Intent;",
                            uri.object(), QJniObject::fromString( mime ).object<jstring>() );
   const QJniObject clip = QJniObject::callStaticObjectMethod(
      "android/content/ClipData", "newRawUri",
      "(Ljava/lang/CharSequence;Landroid/net/Uri;)Landroid/content/ClipData;",
      QJniObject::fromString( QStringLiteral( "video" ) ).object<jstring>(), uri.object() );
   intent.callMethod<void>( "setClipData", "(Landroid/content/ClipData;)V", clip.object() );

   // FLAG_GRANT_READ_URI_PERMISSION, and an explicit grant to every viewer: the chooser alone does not
   // reach every player.
   intent.callObjectMethod( "addFlags", "(I)Landroid/content/Intent;", 0x00000001 );
   const QJniObject manager = context.callObjectMethod( "getPackageManager", "()Landroid/content/pm/PackageManager;" );
   const QJniObject viewers = manager.callObjectMethod(
      "queryIntentActivities", "(Landroid/content/Intent;I)Ljava/util/List;", intent.object(), jint( 0 ) );
   const jint count = viewers.isValid() ? viewers.callMethod<jint>( "size" ) : 0;
   for ( jint i = 0; i < count; ++i ) {
      const QJniObject info = viewers.callObjectMethod( "get", "(I)Ljava/lang/Object;", i );
      const QJniObject activity = info.getObjectField( "activityInfo", "Landroid/content/pm/ActivityInfo;" );
      const QJniObject package = activity.getObjectField<jstring>( "packageName" );
      context.callMethod<void>( "grantUriPermission", "(Ljava/lang/String;Landroid/net/Uri;I)V",
                                package.object<jstring>(), uri.object(), jint( 1 ) );
   }
   env.checkAndClearExceptions();

   const QJniObject chooser = QJniObject::callStaticObjectMethod(
      "android/content/Intent", "createChooser",
      "(Landroid/content/Intent;Ljava/lang/CharSequence;)Landroid/content/Intent;", intent.object(),
      QJniObject::fromString( QStringLiteral( "Open video with" ) ).object<jstring>() );
   // FLAG_ACTIVITY_NEW_TASK: needed when the context is not an activity.
   chooser.callObjectMethod( "addFlags", "(I)Landroid/content/Intent;", 0x10000000 );
   context.callMethod<void>( "startActivity", "(Landroid/content/Intent;)V", chooser.object() );
   return !env.checkAndClearExceptions();
}
