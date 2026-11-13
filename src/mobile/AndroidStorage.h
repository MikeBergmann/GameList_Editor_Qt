#pragma once

#include <QObject>
#include <QStringList>

// All-files access: the one permission the app cannot work without (Android.md, Phase 2).
class AndroidStorage : public QObject
{
   Q_OBJECT
   Q_PROPERTY( bool hasAccess READ hasAccess NOTIFY hasAccessChanged )

public:
   explicit AndroidStorage( QObject* aParent = nullptr );

   bool hasAccess() const { return FHasAccess; }

   // Opens the system page for this app; hasAccess follows when the user comes back.
   Q_INVOKABLE void request();

   // Roots of the mounted non-primary volumes (SD card, USB drive), e.g. /storage/1107-2E5F.
   static QStringList externalVolumes();

   // Offers the file to the apps that can view it, through a content:// address (Android refuses
   // file://). False when the address or the chooser could not be made.
   static bool openFile( const QString& aPath );

signals:
   void hasAccessChanged();

private:
   void refresh();

   bool FHasAccess = false;
};
