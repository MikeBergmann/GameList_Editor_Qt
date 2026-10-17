#pragma once

#include "Gamelist.h"

#include <QString>
#include <QVector>

// Looking for media files and resizing them. These only read a Gamelist; the
// actions that edit it (linkMedia, renameMedia) stay on Gamelist, the one owner
// of the document.

// Files in the image and video folders named after a game's ROM (case-blind,
// optional -image/-video suffix) for games whose link is empty or dead.
// Named after it with or without the ROM's extension ("x.scummvm.png"), in the
// gamelist's own media folders and in ./media/images|videos and ./images|videos.
// A link that works is never proposed for replacement.
QVector<MediaLink> unlinkedMedia( const Gamelist& aList );

// Linked files that exist but are not named after their ROM: the ROM's name
// (with or without its extension) plus nothing, "-image" or "-video", in the
// same case. Those are the names unlinkedMedia accepts, so the two agree.
// Files another game also links, or whose proper name is taken, carry a problem.
// aStrict: only the plain ROM name is right, so "x-image.png" becomes "x.png".
QVector<MediaRename> misnamedMedia( const Gamelist& aList, bool aStrict = false );

// A linked picture that is bigger than the limit the user set.
struct OversizedImage
{
   int index = -1;  // the first game that links it
   QString path;    // absolute path
   qint64 size = 0;  // bytes
};

// Linked pictures that exist and are bigger than aMaxBytes, each file once even
// when several games share it. Shrinking them leaves the gamelist alone: the
// file keeps its name and format, so the links stay right.
QVector<OversizedImage> oversizedImages( const Gamelist& aList, qint64 aMaxBytes );

// Re-encodes the picture in place, in its own format and under the same name, at
// the largest size that fits aMaxBytes (a little under it, so "ca." holds).
// The aspect ratio is kept and it is never enlarged. The value is the new size in
// bytes; an Error leaves the file untouched: unreadable, animated, a format Qt
// cannot write, or one that will not get that small.
Result<qint64> shrinkImageFile( const QString& aPath, qint64 aMaxBytes );
