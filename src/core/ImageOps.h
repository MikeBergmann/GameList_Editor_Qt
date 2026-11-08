#pragma once

#include "Result.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

// Pixel work on picture files; nothing here knows about a Gamelist.

// A linked picture that is bigger than the limit the user set.
struct OversizedImage
{
   int index = -1;  // the first game that links it
   QString path;    // absolute path
   qint64 size = 0;  // bytes
};

// Re-encodes in place, same name and format, to just under aMaxBytes. Aspect
// ratio is kept; never enlarges. The value is the new size. An Error leaves the
// file untouched.
Result<qint64> shrinkImageFile( const QString& aPath, qint64 aMaxBytes );

struct ShrinkReport
{
   int shrunk = 0;
   qint64 saved = 0;      // bytes, over the pictures that were shrunk
   QStringList problems;  // one Error text per picture left as it was
};

// shrinkImageFile over each picture; one that fails is reported and the rest go on.
// aProgress runs before each picture with how many are done; returning false stops there.
ShrinkReport shrinkImages( const QVector<OversizedImage>& aImages, qint64 aMaxBytes,
                           const std::function<bool( int aDone )>& aProgress = {} );
