#pragma once

#include "Result.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

// One file to put on the target. Paths are absolute.
struct RomFile
{
   QString source;
   QString target;
   qint64 size = 0;
};

// What copying one game's ROM would take, with nothing written.
struct RomCopyPlan
{
   QVector<RomFile> files;  // only what is missing or differs on the target
   qint64 bytes = 0;        // the sum of their sizes
   QStringList problems;    // files the ROM needs but cannot be copied (missing, outside the ROM folder)
};


// The files a .cue / .m3u / .gdi names, as physical paths. Empty for any other file.
QStringList discTracks( const QString& aPath );

// aRomPath first, then every file it needs, through nested descriptions (an .m3u of .cue
// sheets). Missing files are listed too; no path is listed twice.
QStringList romParts( const QString& aRomPath );

// aRomLink is the <path> text. The ROM and the files a .cue / .m3u / .gdi names are copied
// when missing on the target or of another size. A folder ROM is a problem, not a copy.
// ponytail: size only, no hash; a same-size changed ROM is not noticed. Upgrade: MD5 when sizes match.
// ponytail: other multi-file formats are copied as the listed file only.
RomCopyPlan planRomCopy( const QString& aSourceDir, const QString& aTargetDir, const QString& aRomLink );

// Each file is staged as *.gle-new and swapped in, so a broken copy never costs the old
// file. aProgress runs before each file; returning false cancels with an Error.
using RomProgress = std::function<bool( const RomFile& )>;
Result<> copyRomFiles( const QVector<RomFile>& aFiles, const RomProgress& aProgress = {} );

// An Error when the volume holding aTargetDir (or its nearest existing parent) has less
// than aBytes free. Passes when the volume cannot say.
Result<> checkFreeSpace( const QString& aTargetDir, qint64 aBytes );
