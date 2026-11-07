#pragma once

#include <QString>

class QNetworkReply;

// errorString() plus the HTTP status code: HTTP/2 has no reason phrase, so errorString() alone
// can name nothing useful. Unsanitized; see sanitizeUrl() before it reaches a dialog.
QString describeError( const QNetworkReply* aReply );
