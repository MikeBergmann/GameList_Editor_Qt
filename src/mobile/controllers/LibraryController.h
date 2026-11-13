#pragma once

#include "Gamelist.h"
#include "Systems.h"

#include <QObject>
#include <QStringList>
#include <QVariantList>

// The ROM root, its systems and the one loaded Gamelist. QML only sees names and indices.
class LibraryController : public QObject
{
   Q_OBJECT
   Q_PROPERTY( QString rootPath READ rootPath NOTIFY systemsChanged )
   Q_PROPERTY( QString lastFolder READ lastFolder NOTIFY systemsChanged )
   Q_PROPERTY( QVariantList systems READ systems NOTIFY systemsChanged )
   Q_PROPERTY( QStringList kindLabels READ kindLabels CONSTANT )
   Q_PROPERTY( int current READ current NOTIFY gamelistChanged )
   Q_PROPERTY( bool loaded READ isLoaded NOTIFY gamelistChanged )
   Q_PROPERTY( QString error READ error NOTIFY gamelistChanged )

public:
   explicit LibraryController( QObject* aParent = nullptr );

   const QString& rootPath() const { return FRootPath; }
   // The root last opened successfully, or empty when it is gone.
   QString lastFolder() const;
   // One map per system: name, logo (a qrc URL), folder.
   const QVariantList& systems() const { return FSystemItems; }
   // "Other (no mapping)" first, then the choices of systemKindChoices().
   QStringList kindLabels() const;
   int current() const { return FCurrent; }
   bool isLoaded() const { return FLoaded; }
   const QString& error() const { return FError; }

   // Null while no system is loaded.
   Gamelist* gamelist() { return FLoaded ? &FGamelist : nullptr; }

   // False (with error()) when there is no system under aRootPath.
   Q_INVOKABLE bool openRoot( const QString& aRootPath );
   // Also reloads the current one. False (with error()) when its gamelist does not load.
   Q_INVOKABLE bool selectSystem( int aIndex );

   // Only a folder the tables do not know can be mapped; a known one would ignore the mapping.
   Q_INVOKABLE bool canSetKind( int aSystem ) const;
   // The index into kindLabels() the system has now.
   Q_INVOKABLE int kindChoice( int aSystem ) const;
   // Index 0 forgets the mapping. Does nothing when canSetKind() is false.
   Q_INVOKABLE void setKind( int aSystem, int aChoice );

signals:
   void systemsChanged();
   // Emitted with gamelist() null before a load and again after it: views must not hold the
   // Gamelist across it.
   void gamelistChanged();

private:
   void rebuildItems();

   QString FRootPath;
   QVector<SystemEntry> FSystems;
   QVector<SystemKindChoice> FChoices = systemKindChoices();
   QVariantList FSystemItems;
   Gamelist FGamelist;
   int FCurrent = -1;
   bool FLoaded = false;
   QString FError;
};
