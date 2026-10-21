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

// Atomic: an Error leaves any existing file at aPath as it was.
Result<> writeFileAtomically( const QString& aPath, const QByteArray& aContent );
