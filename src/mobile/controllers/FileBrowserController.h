#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

#include <functional>

// Picker for QML: the storage roots, then folders, plus files with a chosen suffix. A row is a map
// with name, path and isDir.
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

   // Extra volume roots, asked on every listing of the roots. Android 11+ cannot list /storage, so
   // an SD card there is only found by asking the system.
   void setVolumeSource( std::function<QStringList()> aSource );

   // The roots when aPath is empty or not a folder.
   Q_INVOKABLE void open( const QString& aPath );
   // Lower-case suffixes without the dot, e.g. "png". Empty (the start) lists folders only.
   Q_INVOKABLE void setSuffixes( const QStringList& aSuffixes );
   // To the parent folder, and from a volume's root to the list of roots.
   Q_INVOKABLE void up();

signals:
   void changed();

private:
   QVariantList roots() const;

   QString FStorageDir;
   QString FPath;
   QStringList FSuffixes;
   std::function<QStringList()> FVolumeSource;
   QVariantList FEntries;
};
