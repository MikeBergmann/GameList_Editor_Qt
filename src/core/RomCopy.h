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

// Whether both folders exist and are the same one, symlinks resolved.
bool sameFolder( const QString& aA, const QString& aB );

// aRomLink is the <path> text. The ROM, the files a .cue / .m3u / .gdi names, and every
// file of a folder ROM are copied when missing on the target or of another size.
// ponytail: size only, no hash; a same-size changed ROM is not noticed. Upgrade: MD5 when sizes match.
// ponytail: other multi-file formats are copied as the listed file only.
RomCopyPlan planRomCopy( const QString& aSourceDir, const QString& aTargetDir, const QString& aRomLink );

// Each file is staged as *.gle-new and renamed into place, so a broken copy never leaves
// a half ROM. aProgress runs before each file; returning false cancels with an Error.
using RomProgress = std::function<bool( const RomFile& )>;
Result<> copyRomFiles( const QVector<RomFile>& aFiles, const RomProgress& aProgress = {} );

// An Error when the volume holding aTargetDir (or its nearest existing parent) has less
// than aBytes free. Passes when the volume cannot say.
Result<> checkFreeSpace( const QString& aTargetDir, qint64 aBytes );
