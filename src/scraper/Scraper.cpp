#include "Scraper.h"

#include "DevCredentials.h"

#include <QDomDocument>
#include <QFile>
#include <QHash>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>

#include <utility>

QString decodeDevSecret( const unsigned char* aData )
{
   QByteArray out;
   for ( unsigned i = 0;; ++i ) {
      const char c = static_cast<char>( aData[i] ^ ( ( 0xA5u + 31u * i ) & 0xFFu ) );
      if ( c == '\0' )
         return QString::fromUtf8( out );
      out.append( c );
   }
}

namespace {

// GetFormatedDate (F_Main 3810). The dates node holds ISO "yyyy-mm-dd",
// shortened to "yyyy-mm" or "yyyy" when only part of it is known.
QString formatScrapedDate( const QString& aValue )
{
   switch ( aValue.size() ) {
      case 4: return aValue;
      case 7: return aValue.mid( 5, 2 ) + QLatin1Char( '/' ) + aValue.left( 4 );
      case 10:
         return aValue.mid( 8, 2 ) + QLatin1Char( '/' ) + aValue.mid( 5, 2 ) + QLatin1Char( '/' ) +
                aValue.left( 4 );
      default: return {};
   }
}

// noms/regions/synopsis/dates: a flat dict of <child aAttribute="key">text</child>.
QHash<QString, QString> childDict( const QDomElement& aParent, const char* aAttribute )
{
   QHash<QString, QString> dict;
   for ( QDomElement child = aParent.firstChildElement(); !child.isNull();
         child = child.nextSiblingElement() )
      dict.insert( child.attribute( QLatin1String( aAttribute ) ), child.text() );
   return dict;
}

// TryGetValue's fallback chain: the first key present, even if its value is
// empty - which is what let the Delphi's `or` chain stop on an empty match.
QString pickLocalized( const QHash<QString, QString>& aDict, const QStringList& aFallbackKeys )
{
   for ( const QString& key : aFallbackKeys ) {
      auto it = aDict.constFind( key );
      if ( it != aDict.constEnd() )
         return it.value();
   }
   return {};
}

QStringList descriptionFallback( const QString& aLangStr )
{
   return { aLangStr, QLatin1String( Cst_LangNameStr[lnEnglish] ),
            QLatin1String( Cst_LangNameStr[lnGerman] ), QLatin1String( Cst_LangNameStr[lnSpanish] ),
            QString( QLatin1String( Cst_LangNameStr[lnPortuguese_BR] ) ).left( 2 ) };
}

}  // namespace

bool parseScrapeResponse( const QByteArray& aXml, const QSet<QString>& aMediaTypes, bool aWantVideo,
                          LangName aLanguage, ScrapedInfo& aInfo, QString* aError )
{
   QDomDocument doc;
   QString parseError;
   // ScreenScraper sometimes appends a plain-text note after </Data> (e.g.
   // "CRC non trouve : ..." when the lookup fell back to the ROM name), which a
   // strict parser rejects as extra content. The document itself is complete.
   const qsizetype end = aXml.lastIndexOf( "</Data>" );
   const QByteArray xml = end < 0 ? aXml : aXml.left( end + 7 );
   if ( !doc.setContent( xml, &parseError ) ) {
      if ( aError )
         *aError = parseError;
      return false;
   }

   const QDomElement root = doc.documentElement();
   const QDomElement game = root.firstChildElement( QLatin1String( Cst_GameNode ) );
   if ( game.isNull() ) {
      // 3613: an API error document (no <jeu>) used to raise out of ParseXml
      // with the form left disabled. Report it as a failed scrape instead.
      if ( aError )
         *aError = QStringLiteral( "Game not found" );
      return false;
   }

   const QString langStr = QString( QLatin1String( Cst_LangNameStr[aLanguage] ) ).left( 2 );

   // Name: keyed by region, not by language - "ss" is screenscraper's own
   // generic entry and is tried last, exactly as FillFields did.
   const QDomElement names = game.firstChildElement( QLatin1String( Cst_NamesNode ) );
   aInfo.name =
      pickLocalized( childDict( names, Cst_AttRegion ),
                     { langStr, QLatin1String( Cst_CountryName[cnEu] ),
                       QLatin1String( Cst_CountryName[cnWor] ), QStringLiteral( "ss" ) } );

   // Region: every region the game shipped in, joined for display.
   QStringList regionNames;
   const QDomElement regions = game.firstChildElement( QLatin1String( Cst_RegionsNode ) );
   for ( QDomElement region = regions.firstChildElement(); !region.isNull();
         region = region.nextSiblingElement() )
      regionNames << QLatin1String(
         Cst_CountryNameFull[countryFromShortName( region.text() )][aLanguage] );
   aInfo.region = regionNames.join( QStringLiteral( " - " ) );

   // Description and genre share one fallback chain: aLanguage, then en/de/es/pt.
   const QStringList textFallback = descriptionFallback( langStr );

   const QDomElement synopsis = game.firstChildElement( QLatin1String( Cst_SynopNode ) );
   aInfo.description = pickLocalized( childDict( synopsis, Cst_AttLang ), textFallback );

   const QDomElement dates = game.firstChildElement( QLatin1String( Cst_DateNode ) );
   aInfo.releaseDate = formatScrapedDate( pickLocalized(
      childDict( dates, Cst_AttRegion ),
      { langStr, QLatin1String( Cst_CountryName[cnEu] ), QLatin1String( Cst_CountryName[cnWor] ),
        QLatin1String( Cst_CountryName[cnUs] ), QLatin1String( Cst_CountryName[cnJp] ) } ) );

   // Genres: grouped by id, in the order the document lists them - the
   // Delphi's CreateGenreDict only worked because same-id entries were
   // already adjacent; grouping into a map keyed by id needs no such order.
   QStringList genreIds;
   QHash<QString, QHash<QString, QString>> genresById;
   const QDomElement genres = game.firstChildElement( QLatin1String( Cst_GenreNode ) );
   for ( QDomElement genre = genres.firstChildElement(); !genre.isNull();
         genre = genre.nextSiblingElement() ) {
      const QString id = genre.attribute( QLatin1String( Cst_AttId ) );
      if ( !genresById.contains( id ) )
         genreIds << id;
      genresById[id].insert( genre.attribute( QLatin1String( Cst_AttLang ) ), genre.text() );
   }
   QStringList genreNames;
   for ( const QString& id : std::as_const( genreIds ) )
      genreNames << pickLocalized( genresById.value( id ), textFallback );
   aInfo.genre = genreNames.join( QStringLiteral( " - " ) );

   aInfo.publisher = game.firstChildElement( QLatin1String( Cst_EditNode ) ).text();
   aInfo.developer = game.firstChildElement( QLatin1String( Cst_DevNode ) ).text();
   aInfo.players = game.firstChildElement( QLatin1String( Cst_PlayersNode ) ).text();
   aInfo.rating = game.firstChildElement( QLatin1String( Cst_NoteNode ) ).text();

   // Media: only the checked types, video handled separately since it is
   // offered through Chk_Video/Btn_SaveVideo rather than the picture strip.
   const QDomElement medias = game.firstChildElement( QLatin1String( Cst_MediaNode ) );
   for ( QDomElement media = medias.firstChildElement(); !media.isNull();
         media = media.nextSiblingElement() ) {
      const QString type = media.attribute( QLatin1String( Cst_AttType ) );
      if ( aWantVideo && type == QLatin1String( Cst_MediaVideo ) ) {
         aInfo.videoLink = media.text();
      } else if ( aMediaTypes.contains( type ) ) {
         aInfo.pictures.append(
            { media.attribute( QLatin1String( Cst_AttFormat ) ), media.text() } );
      }
   }

   const QDomElement user = root.firstChildElement( QLatin1String( Cst_UserNode ) );
   const QString threads = user.firstChildElement( QLatin1String( Cst_ThreadNode ) ).text();
   bool threadsOk = false;
   const int maxThreads = threads.toInt( &threadsOk );
   aInfo.maxThreads = ( threadsOk && maxThreads > 0 ) ? maxThreads : 1;

   return true;
}

QString sanitizeUrl( const QString& aUrl )
{
   QString safe = aUrl;
   safe.replace( QRegularExpression( QStringLiteral( "(devid=|devpassword=|ssid=|sspassword=)[^&]*" ) ),
                 QStringLiteral( "\\1" ) );
   return safe;
}

namespace {

constexpr const char* Cst_StreamError = "Oops !! An error has occured while reading the stream !!";

// errorString() reports "... - server replied: <reason phrase>", but HTTP/2
// (which screenscraper.fr uses) has no textual reason phrase, so that half is
// silently empty and the message names nothing useful. The status code is
// still available regardless of protocol version, so append it explicitly.
QString describeError( const QNetworkReply* aReply )
{
   const QVariant status = aReply->attribute( QNetworkRequest::HttpStatusCodeAttribute );
   QString description = aReply->errorString();
   if ( status.isValid() )
      description += QStringLiteral( " (HTTP %1)" ).arg( status.toInt() );
   return description;
}

}  // namespace

namespace {

// Every value is percent-encoded, not just the dev credentials: a password with
// '&', '#', '+' or '%' otherwise ends the parameter early or is read as something
// else, so the login fails (or the query is silently altered) for exactly those users.
QString encoded( const QString& aValue )
{
   return QString::fromUtf8( QUrl::toPercentEncoding( aValue ) );
}

}  // namespace

QString buildGameQuery( const ScrapeRequest& aRequest )
{
   QString query = QLatin1String( Cst_ScraperAddress ) + QLatin1String( Cst_Category ) +
                   QLatin1String( Cst_ScrapeLogin ) + encoded( decodeDevSecret( Cst_DevIdXor ) ) +
                   QLatin1String( Cst_ScrapePwd ) + encoded( decodeDevSecret( Cst_DevPwdXor ) ) +
                   QLatin1String( Cst_DevSoftName ) + QLatin1String( Cst_Output );

   if ( !aRequest.ssLogin.isEmpty() && !aRequest.ssPassword.isEmpty() )
      query += QLatin1String( Cst_SSId ) + encoded( aRequest.ssLogin ) + QLatin1String( Cst_SSPwd ) +
               encoded( aRequest.ssPassword );

   query += QLatin1String( Cst_Crc ) + encoded( aRequest.crc32 ) + QLatin1String( Cst_SystemId ) +
            encoded( aRequest.systemId );

   if ( !aRequest.manualCrc )
      query += QLatin1String( Cst_RomName ) + encoded( aRequest.romName ) +
               QLatin1String( Cst_RomSize ) + QString::number( aRequest.romSize );

   return query;
}

bool devCredentialsConfigured()
{
   return !decodeDevSecret( Cst_DevIdXor ).isEmpty() && !decodeDevSecret( Cst_DevPwdXor ).isEmpty();
}

Scraper::Scraper( QObject* aParent ) : QObject( aParent ) {}

void Scraper::setProxy( const QString& aServer, int aPort, const QString& aUser,
                        const QString& aPassword )
{
   if ( aServer.isEmpty() ) {
      FManager.setProxy( QNetworkProxy::NoProxy );
      return;
   }
   FManager.setProxy( QNetworkProxy( QNetworkProxy::HttpProxy, aServer,
                                     static_cast<quint16>( aPort ), aUser, aPassword ) );
}

// What the server actually sent, for the "Show details" box of the error dialog
// and its Export button. Not truncated: the export is the whole point. The request
// comes first so the details show which systemid/crc/romnom were really sent.
static QString responseDetails( const QString& aQuery, const QByteArray& aBody )
{
   return sanitizeUrl( QStringLiteral( "Request:\n%1\n\nReply:\n%2" )
                          .arg( aQuery, QString::fromUtf8( aBody ) ) );
}

void Scraper::requestGame( const ScrapeRequest& aRequest, const QSet<QString>& aMediaTypes,
                           bool aWantVideo, LangName aLanguage )
{
   if ( !devCredentialsConfigured() ) {
      emit gameInfoFailed( QStringLiteral(
         "This build has no screenscraper.fr developer credentials. Rebuild with "
         "-DGLE_SS_DEVID=... -DGLE_SS_DEVPWD=... (see README)." ) );
      return;
   }

   const QString query = buildGameQuery( aRequest );
   QNetworkReply* reply = FManager.get( QNetworkRequest( QUrl( query ) ) );

   connect( reply, &QNetworkReply::finished, this,
            [this, reply, aMediaTypes, aWantVideo, aLanguage, query] {
               reply->deleteLater();

               if ( reply->error() != QNetworkReply::NoError ) {
                  // screenscraper.fr answers 404 when it has no match for the game.
                  if ( reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt() == 404 ) {
                     emit gameInfoFailed( QStringLiteral( "Game not found" ),
                                          responseDetails( query, reply->readAll() ) );
                     return;
                  }
         // errorString() embeds Qt's own copy of the request URL (credentials
         // and all), so the whole message needs sanitizing, not just query.
                  emit gameInfoFailed(
                     sanitizeUrl( query + QStringLiteral( "\n" ) + describeError( reply ) ),
                     responseDetails( query, reply->readAll() ) );
                  return;
               }

               const QByteArray data = reply->readAll();
               if ( data.isEmpty() ) {
                  emit gameInfoFailed( QLatin1String( Cst_StreamError ) );
                  return;
               }

               ScrapedInfo info;
               QString error;
               if ( !parseScrapeResponse( data, aMediaTypes, aWantVideo, aLanguage, info,
                                          &error ) ) {
                  emit gameInfoFailed( error, responseDetails( query, data ) );
                  return;
               }
               emit gameInfoReady( info );
            } );
}

void Scraper::startNextMedia( const std::shared_ptr<MediaBatch>& aState )
{
   if ( aState->next >= aState->media.size() )
      return;

   const int index = aState->next++;
   aState->active++;

   QNetworkReply* reply = FManager.get( QNetworkRequest( QUrl( aState->media[index].fileLink ) ) );

   connect( reply, &QNetworkReply::finished, this, [this, reply, index, aState] {
      reply->deleteLater();
      aState->active--;

      if ( reply->error() == QNetworkReply::NoError ) {
         QImage image;
         if ( image.loadFromData( reply->readAll() ) )
            emit mediaReady( index, image );
         else
            emit mediaFailed( index, QStringLiteral( "Could not decode image" ) );
      } else {
         emit mediaFailed( index, describeError( reply ) );
      }

      if ( aState->next < aState->media.size() )
         startNextMedia( aState );
      else if ( aState->active == 0 )
         emit mediaFinished();
   } );
}

void Scraper::requestMedia( const QVector<MediaInfo>& aMedia, int aMaxThreads )
{
   if ( aMedia.isEmpty() ) {
      emit mediaFinished();
      return;
   }

   auto state = std::make_shared<MediaBatch>();
   state->media = aMedia;

   const int threads = qBound( 1, aMaxThreads, static_cast<int>( aMedia.size() ) );
   for ( int ii = 0; ii < threads; ++ii )
      startNextMedia( state );
}

void Scraper::downloadToFile( const QString& aUrl, const QString& aDestPath )
{
   QNetworkReply* reply = FManager.get( QNetworkRequest( QUrl( aUrl ) ) );

   connect( reply, &QNetworkReply::finished, this, [this, reply, aUrl, aDestPath] {
      reply->deleteLater();

      if ( reply->error() != QNetworkReply::NoError ) {
         emit fileSaveFailed( aDestPath, sanitizeUrl( aUrl + QStringLiteral( "\n" ) +
                                                      describeError( reply ) ) );
         return;
      }

      const QByteArray data = reply->readAll();
      if ( data.isEmpty() ) {
         emit fileSaveFailed( aDestPath, QLatin1String( Cst_StreamError ) );
         return;
      }

      QFile file( aDestPath );
      if ( !file.open( QIODevice::WriteOnly ) || file.write( data ) != data.size() ) {
         emit fileSaveFailed( aDestPath, file.errorString() );
         return;
      }
      emit fileSaved( aDestPath );
   } );
}
