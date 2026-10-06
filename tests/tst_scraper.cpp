// Scraper: the XML parsing half of jeuInfos.php, against a canned response -
// no network in this test. Covers the per-language fallback chains FillFields
// used to run inline, the media-type filter that replaced the nine
// checkboxes, and the two failure paths (no <jeu>, malformed XML).

#include "QtPrinters.h"
#include "Scraper.h"

#include <gtest/gtest.h>

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
   ScrapedInfo info;
   QString error;
   const QSet<QString> mediaTypes = { QStringLiteral( "box-2D" ), QStringLiteral( "ss" ) };

   ASSERT_TRUE(
      parseScrapeResponse( kResponse, mediaTypes, /*aWantVideo=*/true, lnEnglish, info, &error ) )
      << error.toStdString();

   // Name is keyed by region, not language: English has no "en" region entry,
   // so this falls through to the "eu" region exactly as FillFields did.
   EXPECT_EQ( info.name, QStringLiteral( "Sonic Ball (Europe)" ) );
   EXPECT_EQ( info.region, QStringLiteral( "Europe - USA" ) );
   EXPECT_EQ( info.description, QStringLiteral( "Sonic and Tails team up." ) );
   EXPECT_EQ( info.releaseDate, QStringLiteral( "08/12/1992" ) );
   EXPECT_EQ( info.genre, QStringLiteral( "Platform - Action" ) );
   EXPECT_EQ( info.publisher, QStringLiteral( "Sega" ) );
   EXPECT_EQ( info.developer, QStringLiteral( "Sonic Team" ) );
   EXPECT_EQ( info.players, QStringLiteral( "2" ) );
   EXPECT_EQ( info.rating, QStringLiteral( "18" ) );

   // "wheel" was not in aMediaTypes, so only box-2D and ss made it through.
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
   ScrapedInfo info;
   ASSERT_TRUE( parseScrapeResponse( kResponse, {}, /*aWantVideo=*/false, lnEnglish, info ) );

   EXPECT_TRUE( info.videoLink.isEmpty() );
   EXPECT_TRUE( info.pictures.isEmpty() );
}

TEST( ParseScrapeResponse, GermanPicksItsOwnSynopsisButSpanishGenreFallsToEnglish )
{
   ScrapedInfo german;
   ASSERT_TRUE( parseScrapeResponse( kResponse, {}, false, lnGerman, german ) );
   EXPECT_EQ( german.description, QStringLiteral( "Sonic und Tails." ) );

   ScrapedInfo spanish;
   ASSERT_TRUE( parseScrapeResponse( kResponse, {}, false, lnSpanish, spanish ) );
   // Neither genre id has a Spanish entry, so both fall back to English.
   EXPECT_EQ( spanish.genre, QStringLiteral( "Platform - Action" ) );
}

TEST( ParseScrapeResponse, FailsWithoutAGameNode )
{
   ScrapedInfo info;
   QString error;
   const QByteArray errorDoc = QByteArrayLiteral( "<Data><erreur>Dev login error</erreur></Data>" );

   EXPECT_FALSE( parseScrapeResponse( errorDoc, {}, false, lnEnglish, info, &error ) );
   EXPECT_FALSE( error.isEmpty() );
}

TEST( ParseScrapeResponse, FailsOnMalformedXml )
{
   ScrapedInfo info;
   QString error;

   EXPECT_FALSE(
      parseScrapeResponse( QByteArrayLiteral( "not xml" ), {}, false, lnEnglish, info, &error ) );
   EXPECT_FALSE( error.isEmpty() );
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
   EXPECT_TRUE( safe.contains( QStringLiteral( "devpassword=&" ) ) );
   EXPECT_TRUE( safe.contains( QStringLiteral( "sspassword=&" ) ) );
}

// Self-check for the unconfigured build: must fail fast, offline, with a hint.
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

}  // namespace
