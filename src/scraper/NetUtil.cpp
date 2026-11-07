#include "NetUtil.h"

#include <QNetworkReply>
#include <QNetworkRequest>

QString describeError( const QNetworkReply* aReply )
{
   const QVariant status = aReply->attribute( QNetworkRequest::HttpStatusCodeAttribute );
   QString description = aReply->errorString();
   if ( status.isValid() )
      description += QStringLiteral( " (HTTP %1)" ).arg( status.toInt() );
   return description;
}
