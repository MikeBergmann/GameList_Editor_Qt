#pragma once

#include "Resources.h"

#include <QMetaType>
#include <QString>
#include <QVector>

#include <utility>

// One system folder under the ROM root that holds a gamelist.xml.
struct SystemEntry
{
   SystemEntry() = default;
   SystemEntry( QString aFolderName, SystemKind aKind, QString aGamelistPath, QString aRomDir = {} )
      : folderName( std::move( aFolderName ) ), kind( aKind ), gamelistPath( std::move( aGamelistPath ) ),
        romDir( std::move( aRomDir ) )
   {
   }

   QString folderName;
   SystemKind kind = skOther;
   QString gamelistPath;
   QString romDir;  // base of <path> and media links; empty: the gamelist's folder
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

// The combo text: the table name, plus the folder name when it differs (a mapped folder).
// aGenesis shows the Mega Drive under its US name.
QString systemDisplayName( const SystemEntry& aSystem, bool aGenesis );

Q_DECLARE_METATYPE( SystemEntry )
