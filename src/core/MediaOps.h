#pragma once

#include "Gamelist.h"

#include "ImageOps.h"

#include <QString>
#include <QStringList>
#include <QVector>

// Helpers over a Gamelist's files: finders, plus discarding old backups. The edits to
// the gamelist itself live on Gamelist.

// Games whose ROM is not on disk.
QVector<int> orphanIndexes( const Gamelist& aList );

// The later entries of each ROM listed more than once. Same file, not same name.
QVector<int> duplicateIndexes( const Gamelist& aList );

// Absolute paths of unlisted ROM candidates, sorted. Media is excluded.
// A folder ROM ("dig.scummvm") is one candidate; its contents are not.
QStringList unlistedRoms( const Gamelist& aList );

// Files named after a game's ROM, for games whose link is empty or dead.
QVector<MediaLink> unlinkedMedia( const Gamelist& aList );

// Linked files that exist but are not named after their ROM. Entries that must
// not be renamed carry a problem. aStrict accepts only the plain ROM name.
QVector<MediaRename> misnamedMedia( const Gamelist& aList, bool aStrict = false );

// The session backups Gamelist::save leaves next to the gamelist, newest first.
QStringList gamelistBackups( const Gamelist& aList );

// Discards all but the newest aKeep backups (to the trash when there is one). The
// value is how many went; an Error names those that could not be discarded.
Result<int> deleteOldBackups( const Gamelist& aList, int aKeep );

// Linked pictures larger than aMaxBytes, each file once even when shared.
QVector<OversizedImage> oversizedImages( const Gamelist& aList, qint64 aMaxBytes );
