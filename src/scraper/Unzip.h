#pragma once

#include "Result.h"

#include <QString>
#include <QStringList>

// Extracts the named entries of a zip into aDestDir, each written atomically and only once all
// of them inflated and matched their CRC. Stored and deflated entries; no zip64, no encryption.
Result<> unzipEntries( const QString& aZipPath, const QStringList& aNames, const QString& aDestDir );
