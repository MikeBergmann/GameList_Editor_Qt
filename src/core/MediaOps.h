#pragma once

#include "Gamelist.h"

#include <QString>
#include <QVector>

// Read-only helpers over a Gamelist. The edits themselves live on Gamelist.

// Games whose ROM is not on disk.
QVector<int> orphanIndexes( const Gamelist& aList );

// The later entries of each ROM listed more than once. Same file, not same name.
QVector<int> duplicateIndexes( const Gamelist& aList );

// Absolute paths of unlisted ROM candidates, sorted. Media is excluded.
QStringList unlistedRoms( const Gamelist& aList );

// Files named after a game's ROM, for games whose link is empty or dead.
QVector<MediaLink> unlinkedMedia( const Gamelist& aList );

// Linked files that exist but are not named after their ROM. Entries that must
// not be renamed carry a problem. aStrict accepts only the plain ROM name.
QVector<MediaRename> misnamedMedia( const Gamelist& aList, bool aStrict = false );

// A linked picture that is bigger than the limit the user set.
struct OversizedImage
{
   int index = -1;  // the first game that links it
   QString path;    // absolute path
   qint64 size = 0;  // bytes
};

// Linked pictures larger than aMaxBytes, each file once even when shared.
QVector<OversizedImage> oversizedImages( const Gamelist& aList, qint64 aMaxBytes );

// Re-encodes in place, same name and format, to just under aMaxBytes. Aspect
// ratio is kept; never enlarges. The value is the new size. An Error leaves the
// file untouched.
Result<qint64> shrinkImageFile( const QString& aPath, qint64 aMaxBytes );
