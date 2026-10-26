#pragma once

#include "Resources.h"

#include <QMetaType>
#include <QString>
#include <QVector>

#include <utility>

// One system folder that holds a gamelist.xml: under the ROM root, or (ES-DE) under
// the app-data folder's gamelists/, with the ROMs in romDir.
struct SystemEntry
{
   SystemEntry() = default;
   SystemEntry( QString aFolderName, SystemKind aKind, QString aGamelistPath, QString aRomDir = {},
                 QString aMediaDir = {} )
      : folderName( std::move( aFolderName ) ), kind( aKind ), gamelistPath( std::move( aGamelistPath ) ),
        romDir( std::move( aRomDir ) ), mediaDir( std::move( aMediaDir ) )
   {
   }

   QString folderName;
   SystemKind kind = skOther;
   QString gamelistPath;
   QString romDir;    // base of <path> and media links; empty: the gamelist's folder
   QString mediaDir;  // ES-DE: media found by the ROM's name, <image>/<video> ignored; empty: by link
};

// aRootPath is a ROM root (a gamelist.xml per system folder) or an ES-DE app-data folder
// (gamelists/<system>/gamelist.xml). Systems with no gamelist are left out.
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
