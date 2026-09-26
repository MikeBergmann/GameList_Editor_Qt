#pragma once

// Without this a failed QString comparison prints as a raw byte dump.

#include <QString>

#include <ostream>

inline void PrintTo( const QString& aString, std::ostream* aStream )
{
   *aStream << '"' << aString.toUtf8().constData() << '"';
}
