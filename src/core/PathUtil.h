#pragma once

#include "Result.h"

#include <QByteArray>
#include <QString>

// Gamelists often come from Windows, so a link's case may not match the disk.
// This is the one choke point that falls back to a case-insensitive match.
QString resolvePath( const QString& aPath );

// The file aRelative (a link as the gamelist writes it) names under aSystemDir, with
// its case settled against the disk. Empty for an empty link.
QString physicalPath( const QString& aSystemDir, const QString& aRelative );

// Probed, not assumed from the OS: Linux can mount NTFS, Windows has case-sensitive folders.
bool volumeFoldsCase( const QString& aGamelistPath );

// A key for sets: a bare toLower() is wrong on Linux, where sonic.zip is not Sonic.zip.
QString fileKey( const QString& aPath, bool aFoldsCase );

// Both paths are already clean, as every physical path is.
bool sameFile( const QString& aA, const QString& aB, bool aFoldsCase );

// ponytail: canonicalises only same-named paths, to stay cheap; a symlinked file under
// another name is missed. Upgrade: compare file identity per entry.
bool sameFileOnDisk( const QString& aA, const QString& aB, bool aFoldsCase );

// Removes a file the user asked to delete: into the trash, or for good where there is
// none (a network share). False when the file is still there.
bool discardFile( const QString& aPath );

// Tests turn the trash off so they never fill the real one. On by default.
void setTrashEnabled( bool aEnabled );

// Atomic: an Error leaves any existing file at aPath as it was.
Result<> writeFileAtomically( const QString& aPath, const QByteArray& aContent );
