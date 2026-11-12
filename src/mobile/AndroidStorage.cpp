#include "AndroidStorage.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QJniEnvironment>
#include <QJniObject>

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
