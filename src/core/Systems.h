#pragma once

#include "Resources.h"

#include <QMetaType>
#include <QString>
#include <QVector>

// One system folder under the ROM root that holds a gamelist.xml.
struct SystemEntry
{
   QString folderName;
   SystemKind kind = skOther;
   QString gamelistPath;
};

QVector<SystemEntry> scanSystems( const QString& aRootPath );

Q_DECLARE_METATYPE( SystemEntry )
