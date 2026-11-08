#pragma once

#include <QObject>
#include <QVariantList>

// Folder picker for QML: the storage roots, then folders only. A row is a map with name and path.
class FileBrowserController : public QObject
{
   Q_OBJECT
   Q_PROPERTY( QString path READ path NOTIFY changed )
   Q_PROPERTY( bool atRoots READ atRoots NOTIFY changed )
   Q_PROPERTY( QVariantList entries READ entries NOTIFY changed )

public:
   // aStorageDir holds the volumes: emulated/0 is the internal storage, each other folder an SD card.
   explicit FileBrowserController( QString aStorageDir = QStringLiteral( "/storage" ),
                                   QObject* aParent = nullptr );

   const QString& path() const { return FPath; }
   bool atRoots() const { return FPath.isEmpty(); }
   const QVariantList& entries() const { return FEntries; }

   // The roots when aPath is empty or not a folder.
   Q_INVOKABLE void open( const QString& aPath );
   // To the parent folder, and from a volume's root to the list of roots.
   Q_INVOKABLE void up();

signals:
   void changed();

private:
   QVariantList roots() const;

   QString FStorageDir;
   QString FPath;
   QVariantList FEntries;
};
