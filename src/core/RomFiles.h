#pragma once

#include <QString>
#include <QStringList>

// What a ROM is made of on disk. A .cue / .m3u / .gdi names the files it needs; nothing here
// knows about a Gamelist or about copying.

// aRomPath first, then every file it needs, through nested descriptions (an .m3u of .cue
// sheets). Missing files are listed too; no path is listed twice.
QStringList romParts( const QString& aRomPath );

// The files aRomPath's description names that exist inside aRomDir and that no ROM in
// aListedRoms needs. Never aRomPath itself. All paths are physical.
QStringList exclusiveTracks( const QString& aRomPath, const QStringList& aListedRoms,
                             const QString& aRomDir, bool aFoldsCase );
