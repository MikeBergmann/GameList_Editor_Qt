// Scraper: parsing of the ScreenScraper response against a canned reply (no network),
// covering the per-language fallback chains, the media-type filter, the failure paths,
// password scrubbing and query encoding.

#include "QtPrinters.h"
#include "Scraper.h"

#include <gtest/gtest.h>

#include <QUrl>
#include <QUrlQuery>

namespace {

// One <jeu>: two names keyed by region (not language - "ss" is
// screenscraper's own generic entry), two regions, bilingual synopsis/dates,
// two genres each with two languages, and three media, one of them a video.
const QByteArray kResponse = QByteArrayLiteral( R"(<?xml version="1.0" encoding="UTF-8"?>
<Data>
   <jeu id="3244">
      <noms>
         <nom region="wor">Sonic Ball</nom>
         <nom region="eu">Sonic Ball (Europe)</nom>
      </noms>
      <regions>
         <region>eu</region>
         <region>us</region>
      </regions>
      <synopsis>
         <synopsis langue="en">Sonic and Tails team up.</synopsis>
         <synopsis langue="de">Sonic und Tails.</synopsis>
      </synopsis>
      <dates>
         <date region="eu">1992-12-08</date>
         <date region="us">1992-11-24</date>
      </dates>
      <genres>
         <genre id="1" langue="en">Platform</genre>
         <genre id="1" langue="fr">Plateforme</genre>
         <genre id="2" langue="en">Action</genre>
      </genres>
      <editeur>Sega</editeur>
      <developpeur>Sonic Team</developpeur>
      <joueurs>2</joueurs>
      <note>18</note>
      <medias>
         <media type="box-2D" format="png">https://example.invalid/box2d.png</media>
         <media type="video" format="mp4">https://example.invalid/video.mp4</media>
         <media type="ss" format="png">https://example.invalid/screenshot.png</media>
         <media type="wheel" format="png">https://example.invalid/wheel.png</media>
      </medias>
   </jeu>
   <ssuser>
      <maxthreads>3</maxthreads>
   </ssuser>
</Data>
)" );

TEST( ParseScrapeResponse, ResolvesEnglishFieldsAndFiltersMedia )
{
   const QSet<QString> mediaTypes = { QStringLiteral( "box-2D" ), QStringLiteral( "ss" ) };

   const Result<ScrapedInfo> parsed =
      parseScrapeResponse( kResponse, mediaTypes, /*aWantVideo=*/true, lnEnglish );
   ASSERT_TRUE( parsed ) << parsed.error().toStdString();
   const ScrapedInfo& info = parsed.value();

   // Name is keyed by region, not language: English has no "en" region entry,
   // so this falls through to the "eu" region.
   EXPECT_EQ( info.name, QStringLiteral( "Sonic Ball (Europe)" ) );
   EXPECT_EQ( info.region, QStringLiteral( "Europe - USA" ) );
   EXPECT_EQ( info.description, QStringLiteral( "Sonic and Tails team up." ) );
   EXPECT_EQ( info.releaseDate, QStringLiteral( "08/12/1992" ) );
   EXPECT_EQ( info.genre, QStringLiteral( "Platform - Action" ) );
   EXPECT_EQ( info.publisher, QStringLiteral( "Sega" ) );
   EXPECT_EQ( info.developer, QStringLiteral( "Sonic Team" ) );
   EXPECT_EQ( info.players, QStringLiteral( "2" ) );
   EXPECT_EQ( info.rating, QStringLiteral( "18" ) );

   // "wheel" was not in mediaTypes, so only box-2D and ss made it through.
   ASSERT_EQ( info.pictures.size(), 2 );
   EXPECT_EQ( info.pictures.at( 0 ).fileLink,
              QStringLiteral( "https://example.invalid/box2d.png" ) );
   EXPECT_EQ( info.pictures.at( 1 ).fileLink,
              QStringLiteral( "https://example.invalid/screenshot.png" ) );
   EXPECT_EQ( info.videoLink, QStringLiteral( "https://example.invalid/video.mp4" ) );
   EXPECT_EQ( info.maxThreads, 3 );
}

TEST( ParseScrapeResponse, IgnoresVideoWhenNotRequested )
{
   const Result<ScrapedInfo> parsed =
      parseScrapeResponse( kResponse, {}, /*aWantVideo=*/false, lnEnglish );
   ASSERT_TRUE( parsed );

   EXPECT_TRUE( parsed.value().videoLink.isEmpty() );
   EXPECT_TRUE( parsed.value().pictures.isEmpty() );
}

TEST( ParseScrapeResponse, GermanPicksItsOwnSynopsisButSpanishGenreFallsToEnglish )
{
   const Result<ScrapedInfo> german = parseScrapeResponse( kResponse, {}, false, lnGerman );
   ASSERT_TRUE( german );
   EXPECT_EQ( german.value().description, QStringLiteral( "Sonic und Tails." ) );

   const Result<ScrapedInfo> spanish = parseScrapeResponse( kResponse, {}, false, lnSpanish );
   ASSERT_TRUE( spanish );
   // Neither genre id has a Spanish entry, so both fall back to English.
   EXPECT_EQ( spanish.value().genre, QStringLiteral( "Platform - Action" ) );
}

TEST( ParseScrapeResponse, FailsWithoutAGameNode )
{
   const QByteArray errorDoc = QByteArrayLiteral( "<Data><erreur>Dev login error</erreur></Data>" );

   const Result<ScrapedInfo> parsed = parseScrapeResponse( errorDoc, {}, false, lnEnglish );
   EXPECT_FALSE( parsed );
   EXPECT_FALSE( parsed.error().isEmpty() );
}

TEST( ParseScrapeResponse, IgnoresATextNoteAfterTheDocument )
{
   // Seen from the live API: the name fallback appends a French note after </Data>.
   const QByteArray reply = kResponse + "\nCRC non trouv\xC3\xA9 : Champ systemeid obligatoire  \n";

   const Result<ScrapedInfo> parsed = parseScrapeResponse( reply, {}, false, lnEnglish );
   ASSERT_TRUE( parsed ) << parsed.error().toStdString();
   EXPECT_EQ( parsed.value().name, QStringLiteral( "Sonic Ball (Europe)" ) );
}

TEST( ParseScrapeResponse, FailsOnMalformedXml )
{
   const Result<ScrapedInfo> parsed =
      parseScrapeResponse( QByteArrayLiteral( "not xml" ), {}, false, lnEnglish );
   EXPECT_FALSE( parsed );
   EXPECT_FALSE( parsed.error().isEmpty() );
}

// QNetworkReply::errorString() embeds its own copy of the request URL
// ("Error transferring <url> - server replied: ...") - both the dev password
// and the user's own sspassword must still be scrubbed when that whole
// message is sanitized, not just the URL the caller built.
TEST( SanitizeUrl, ScrubsBothDevAndUserPasswordAnywhereInTheMessage )
{
   const QString message =
      QStringLiteral( "Error transferring https://www.screenscraper.fr/api2/jeuInfos.php"
                      "?devid=Frogger&devpassword=Galaga&ssid=Pitfall&sspassword=Asteroids"
                      "&crc=DFCAE7E1 - server replied: Not Found" );

   const QString safe = sanitizeUrl( message );

   EXPECT_FALSE( safe.contains( QStringLiteral( "Galaga" ) ) );
   EXPECT_FALSE( safe.contains( QStringLiteral( "Asteroids" ) ) );
   EXPECT_FALSE( safe.contains( QStringLiteral( "Frogger" ) ) );
   EXPECT_FALSE( safe.contains( QStringLiteral( "Pitfall" ) ) );
   EXPECT_TRUE( safe.contains( QStringLiteral( "devid=&" ) ) );
   EXPECT_TRUE( safe.contains( QStringLiteral( "ssid=&" ) ) );
   EXPECT_TRUE( safe.contains( QStringLiteral( "crc=DFCAE7E1" ) ) );
   EXPECT_TRUE( safe.contains( QStringLiteral( "devpassword=&" ) ) );
   EXPECT_TRUE( safe.contains( QStringLiteral( "sspassword=&" ) ) );
}

// A password such as p&ss#w+rd%41 used to go into the URL raw: '&' ended the value,
// '#' started a fragment and dropped everything after it (crc, systemid, romnom), '+'
// read as a space. The only reliable check is to parse the URL back the way the server
// does and compare every value with what went in.
TEST( Scraper, QueryRoundTripsValuesWithReservedCharacters )
{
   ScrapeRequest request;
   request.systemId = QStringLiteral( "64" );
   request.crc32 = QStringLiteral( "BCD9EC52" );
   request.romName = QStringLiteral( "Sonic & Knuckles #2 (v1+2) 100%.zip" );
   request.romSize = 901120;
   request.ssLogin = QStringLiteral( "user&name=x" );
   request.ssPassword = QStringLiteral( "p&ss#w+rd%41 \u00e4" );

   const QUrl url( buildGameQuery( request ) );
   const QUrlQuery query( url );
   const auto value = [&]( const char* aKey ) {
      return query.queryItemValue( QLatin1String( aKey ), QUrl::FullyDecoded );
   };

   EXPECT_TRUE( url.fragment().isEmpty() );
   EXPECT_EQ( value( "ssid" ), request.ssLogin );
   EXPECT_EQ( value( "sspassword" ), request.ssPassword );
   EXPECT_EQ( value( "crc" ), request.crc32 );
   EXPECT_EQ( value( "systemid" ), request.systemId );
   EXPECT_EQ( value( "romnom" ), request.romName );
   EXPECT_EQ( value( "romtaille" ), QStringLiteral( "901120" ) );
}

TEST( Scraper, QueryOmitsUserLoginWhenEitherHalfIsMissing )
{
   ScrapeRequest request;
   request.ssLogin = QStringLiteral( "user" );

   EXPECT_FALSE( QUrlQuery( QUrl( buildGameQuery( request ) ) ).hasQueryItem( QStringLiteral( "ssid" ) ) );
}

// Bytes worked out by hand from key(i) = (0xA5 + 31*i) & 0xFF, not by calling the encoder,
// so a change to either side of the CMake/C++ pair shows up here.
TEST( Scraper, DecodesXorEncodedDevSecret )
{
   const unsigned char abc[] = { 0xC4, 0xA6, 0x80, 0x02 };
   const unsigned char empty[] = { 0xA5 };

   EXPECT_EQ( decodeDevSecret( abc ), QStringLiteral( "abc" ) );
   EXPECT_TRUE( decodeDevSecret( empty ).isEmpty() );
}

// An unconfigured build must fail fast, offline, with a hint.
TEST( Scraper, FailsWithoutDevCredentials )
{
   if ( devCredentialsConfigured() )
      GTEST_SKIP() << "built with dev credentials";

   Scraper scraper;
   QString message;
   QObject::connect( &scraper, &Scraper::gameInfoFailed,
                     [&]( const QString& aMessage ) { message = aMessage; } );

   scraper.requestGame( ScrapeRequest{}, {}, false, lnEnglish );

   EXPECT_TRUE( message.contains( QStringLiteral( "GLE_SS_DEVID" ) ) );
}

TEST( Scraper, RequestsTimeOut )
{
   Scraper scraper;
   const QNetworkAccessManager* manager = scraper.findChild<QNetworkAccessManager*>();
   ASSERT_NE( manager, nullptr );
   EXPECT_GT( manager->transferTimeout(), 0 )
      << "a stalled reply would keep the scrape panel, and so the window, busy for good";
}

}  // namespace
