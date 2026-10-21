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

// Only folders the tables do not know (skOther) are looked up in the user's mapping.
void applySystemMap( QVector<SystemEntry>& aSystems );

// skOther forgets the mapping.
void setSystemMapping( const QString& aFolderName, SystemKind aKind );

// The table names repeat (two "Colecovision", two "PC Engine"), so the label carries the
// ES folder name, which is unique.
struct SystemKindChoice
{
   QString label;
   SystemKind kind;
};
QVector<SystemKindChoice> systemKindChoices();

Q_DECLARE_METATYPE( SystemEntry )
