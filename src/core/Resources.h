#pragma once

#include <QString>

// Lookup tables and constants: system kinds, countries, languages, gamelist.xml node names,
// settings keys and screenscraper.fr request/reply names.
//
// Cst_ prefixes tables and constants; sk/ln/cn prefix the enumerators.

// Unscoped on purpose: every enumerator below is used as an array subscript.
enum SystemKind
{
   skNES,
   skSNES,
   skMasterSystem,
   skMegaDrive,
   skNeoGeo,
   skCPC,
   skAT2600,
   skAT7800,
   skATST,
   skCS,
   skFCD,
   skFBA,
   skFBALib,
   skGW,
   skGBC,
   skGG,
   skGB,
   skGBA,
   skLU,
   skLYNX,
   skMAME,
   skMSX,
   skMSX1,
   skMSX2,
   skNGP,
   skNGPC,
   skN64,
   skODY,
   skPCE,
   skPCECD,
   skPS,
   skPRB,
   skSVM,
   skS32X,
   skSCD,
   skSG1000,
   skSGFX,
   skVCX,
   skVB,
   skWS,
   skWSC,
   skZXS,
   skZX81,
   skAM1200,
   skAM600,
   skAPPLE,
   skCV,
   skC64,
   skDB,
   skDC,
   skGC,
   skPSP,
   skWII,
   skGenesis,
   sk3do,
   skAM,
   skAMCD,
   skARC,
   skAT5200,
   skATLX,
   skCDTV,
   skCV2,
   skDaphne,
   skGX4000,
   skIV,
   skNaomi,
   skNGCD,
   skOB,
   skPorts,
   skSS,
   skSNESCD,
   skX68000,
   skAW,
   skTI99,
   skMSU1,
   skGW2,
   skNDS,
   skODY2,
   skTG,
   skTGCD,
   skPSPM,
   skWindows,
   skAndroid,
   skOther,
   skCount
};

enum LangName
{
   lnFrench,
   lnGerman,
   lnEnglish,
   lnSpanish,
   lnPortuguese_BR,
   lnCount
};

enum CountryName
{
   cnUnd,
   cnDe,
   cnAsi,
   cnAu,
   cnBr,
   cnBg,
   cnCa,
   cnCl,
   cnCn,
   cnAme,
   cnKr,
   cnCus,
   cnDk,
   cnSp,
   cnEu,
   cnFi,
   cnFr,
   cnGr,
   cnHu,
   cnIl,
   cnIt,
   cnJp,
   cnKw,
   cnWor,
   cnMor,
   cnNo,
   cnNz,
   cnOce,
   cnNl,
   cnPe,
   cnPl,
   cnPt,
   cnCz,
   cnUk,
   cnRu,
   cnSk,
   cnSe,
   cnTw,
   cnTr,
   cnUs,
   cnCount
};

// Country names stay const char*: they are UTF-8, which QLatin1StringView would mangle.

struct SystemInfo
{
   SystemKind kind;
   // Display name.
   const char* name;
   // Per-system folder under the ROM root. Empty for skOther.
   const char* folder;
   // File name under Cst_LogoPicsFolder. Not unique: several kinds share a logo.
   const char* logo;
   // screenscraper.fr system id (systemesListe.php). Not unique either.
   const char* ssId;
   // LaunchBox platform name, as Metadata.xml writes it. Empty where LaunchBox has none.
   // Mega Drive and Genesis share "Sega Genesis"; MAME-like kinds all map to "Arcade".
   const char* lbName;
};

// The row of aKind; the table is checked to follow the enum order.
const SystemInfo& systemInfo( SystemKind aKind );

// screenscraper.fr region/country codes (mostly ISO, also asi, wor, oce). Cst_CountryName[cnUnd] is "".
extern const char* const Cst_CountryName[cnCount];
// Country name per UI language, indexed [country][lang].
extern const char* const Cst_CountryNameFull[cnCount][lnCount];

// Language codes, as sent to screenscraper.fr and matched against its replies.
extern const char* const Cst_LangNameStr[lnCount];

// Reverse lookups. Each falls back to the catch-all rather than failing: an
// unrecognised folder is just a system this build does not know about.
SystemKind systemKindFromFolder( const QString& aFolderName );
CountryName countryFromShortName( const QString& aShortName );
LangName langFromIndex( int aNumber );

// --- gamelist.xml: node and attribute names -------------------------------

inline constexpr QLatin1StringView Cst_GameListFileName{ "gamelist.xml" };
inline constexpr QLatin1StringView Cst_Game{ "game" };
inline constexpr QLatin1StringView Cst_Path{ "path" };
inline constexpr QLatin1StringView Cst_Name{ "name" };
inline constexpr QLatin1StringView Cst_Description{ "desc" };
inline constexpr QLatin1StringView Cst_ImageLink{ "image" };
inline constexpr QLatin1StringView Cst_VideoLink{ "video" };
inline constexpr QLatin1StringView Cst_Rating{ "rating" };
inline constexpr QLatin1StringView Cst_ReleaseDate{ "releasedate" };
inline constexpr QLatin1StringView Cst_Developer{ "developer" };
inline constexpr QLatin1StringView Cst_Publisher{ "publisher" };
inline constexpr QLatin1StringView Cst_Genre{ "genre" };
inline constexpr QLatin1StringView Cst_Players{ "players" };
inline constexpr QLatin1StringView Cst_Region{ "region" };
inline constexpr QLatin1StringView Cst_Playcount{ "playcount" };
inline constexpr QLatin1StringView Cst_LastPlayed{ "lastplayed" };
inline constexpr QLatin1StringView Cst_KidGame{ "kidgame" };
inline constexpr QLatin1StringView Cst_Hidden{ "hidden" };
inline constexpr QLatin1StringView Cst_Favorite{ "favorite" };
inline constexpr QLatin1StringView Cst_True{ "true" };
inline constexpr QLatin1StringView Cst_False{ "false" };

// <releasedate> is yyyymmddT000000; short fields are padded out to these.
inline constexpr QLatin1StringView Cst_DateShortFill{ "00" };
inline constexpr QLatin1StringView Cst_DateLongFill{ "0000" };
inline constexpr QLatin1StringView Cst_DateSuffix{ "T000000" };

inline constexpr QLatin1StringView Cst_ImageSuffixPng{ ".png" };
inline constexpr QLatin1StringView Cst_VideoSuffixMp4{ ".mp4" };

// Scrapers may append these to the ROM name: Sonic-image.png, Sonic-video.mp4.
inline constexpr QLatin1StringView Cst_ImageDash{ "-image" };
inline constexpr QLatin1StringView Cst_VideoDash{ "-video" };
inline constexpr QLatin1StringView Cst_TxtExtension{ ".txt" };

// --- bundled assets -------------------------------------------------------

// .qrc prefixes, not filesystem paths - see Resources/assets.qrc. QPixmap reads
// ":/" transparently, POSIX calls do not.
inline constexpr QLatin1StringView Cst_DefaultPicsFolderPath{ ":/DefaultPictures/" };
inline constexpr QLatin1StringView Cst_LogoPicsFolder{ ":/SystemsLogos/" };
inline constexpr QLatin1StringView Cst_DefaultImageName{ "default.png" };

// --- QSettings: group and keys --------------------------------------------

inline constexpr QLatin1StringView Cst_IniOptions{ "Options" };
inline constexpr QLatin1StringView Cst_IniGodMode{ "GodMode" };
inline constexpr QLatin1StringView Cst_IniAutoHash{ "AutoHash" };
inline constexpr QLatin1StringView Cst_IniDelWoPrompt{ "DelWoPrompt" };
inline constexpr QLatin1StringView Cst_ShowTips{ "ShowTips" };
inline constexpr QLatin1StringView Cst_IniGenesisLogo{ "GenesisLogo" };
inline constexpr QLatin1StringView Cst_IniAutoplayVideo{ "AutoplayVideo" };
inline constexpr QLatin1StringView Cst_IniLanguage{ "Language" };
inline constexpr QLatin1StringView Cst_IniSSUser{ "SSUser" };
inline constexpr QLatin1StringView Cst_IniSSPwd{ "SSPwd" };
inline constexpr QLatin1StringView Cst_IniProxyUser{ "ProxyUser" };
inline constexpr QLatin1StringView Cst_IniProxyPwd{ "ProxyPwd" };
inline constexpr QLatin1StringView Cst_IniProxyServer{ "ProxyServer" };
inline constexpr QLatin1StringView Cst_IniProxyPort{ "ProxyPort" };
inline constexpr QLatin1StringView Cst_IniProxyUse{ "ProxyUse" };
inline constexpr QLatin1StringView Cst_IniLastFolder{ "LastFolder" };

inline constexpr QLatin1StringView Cst_IniWindow{ "Window" };
inline constexpr QLatin1StringView Cst_IniGeometry{ "Geometry" };
inline constexpr QLatin1StringView Cst_IniSplitMain{ "SplitMain" };
// Folder name -> known system's folder name ("amiga500" = "amiga"), for
// folders the tables do not recognise.
inline constexpr QLatin1StringView Cst_IniSystemMap{ "SystemMap" };
// Validators of the cached LaunchBox dump, sent back as If-None-Match / If-Modified-Since.
inline constexpr QLatin1StringView Cst_IniLaunchBoxETag{ "LaunchBoxETag" };
inline constexpr QLatin1StringView Cst_IniLaunchBoxModified{ "LaunchBoxModified" };
// Scrape tab source: 0 ScreenScraper (the default), 1 LaunchBox.
inline constexpr QLatin1StringView Cst_IniScrapeSource{ "ScrapeSource" };

// Language is the one key whose default is not the zero value (lnFrench is 0).
inline constexpr int Cst_IniLanguageDefault = lnEnglish;

// --- screenscraper.fr -----------------------------------------------------

inline constexpr QLatin1StringView Cst_ScraperAddress{ "https://www.screenscraper.fr/api2/" };
inline constexpr QLatin1StringView Cst_Category{ "jeuInfos.php" };
inline constexpr QLatin1StringView Cst_SearchCategory{ "jeuRecherche.php" };
inline constexpr QLatin1StringView Cst_ScrapeLogin{ "?devid=" };
inline constexpr QLatin1StringView Cst_ScrapePwd{ "&devpassword=" };

inline constexpr QLatin1StringView Cst_DevSoftName{ "&softname=GameListEditorQtv1" };
inline constexpr QLatin1StringView Cst_Output{ "&output=xml" };
inline constexpr QLatin1StringView Cst_SSId{ "&ssid=" };
inline constexpr QLatin1StringView Cst_SSPwd{ "&sspassword=" };
inline constexpr QLatin1StringView Cst_Crc{ "&crc=" };
inline constexpr QLatin1StringView Cst_SystemId{ "&systemeid=" };
inline constexpr QLatin1StringView Cst_RomName{ "&romnom=" };
inline constexpr QLatin1StringView Cst_RomSize{ "&romtaille=" };
inline constexpr QLatin1StringView Cst_SearchText{ "&recherche=" };

// What Scraper reports when the server has no match; ScrapePanel offers a search on it.
inline constexpr QLatin1StringView Cst_GameNotFound{ "Game not found" };


// Reply nodes. French, because the API is.
inline constexpr QLatin1StringView Cst_DataNode{ "Data" };
inline constexpr QLatin1StringView Cst_GameNode{ "jeu" };
inline constexpr QLatin1StringView Cst_GamesNode{ "jeux" };
inline constexpr QLatin1StringView Cst_UserNode{ "ssuser" };
inline constexpr QLatin1StringView Cst_ThreadNode{ "maxthreads" };
inline constexpr QLatin1StringView Cst_MediaNode{ "medias" };
inline constexpr QLatin1StringView Cst_NamesNode{ "noms" };
inline constexpr QLatin1StringView Cst_RegionsNode{ "regions" };
inline constexpr QLatin1StringView Cst_EditNode{ "editeur" };
inline constexpr QLatin1StringView Cst_DevNode{ "developpeur" };
inline constexpr QLatin1StringView Cst_NoteNode{ "note" };
inline constexpr QLatin1StringView Cst_PlayersNode{ "joueurs" };
inline constexpr QLatin1StringView Cst_SynopNode{ "synopsis" };
inline constexpr QLatin1StringView Cst_DateNode{ "dates" };
inline constexpr QLatin1StringView Cst_GenreNode{ "genres" };
inline constexpr QLatin1StringView Cst_AttType{ "type" };
inline constexpr QLatin1StringView Cst_AttFormat{ "format" };
inline constexpr QLatin1StringView Cst_AttRegion{ "region" };
inline constexpr QLatin1StringView Cst_AttLang{ "langue" };
inline constexpr QLatin1StringView Cst_AttId{ "id" };
inline constexpr QLatin1StringView Cst_PngExt{ "png" };

// <media type="..."> values the scraper offers, in the order the scrape panel shows them.
inline constexpr QLatin1StringView Cst_MediaBox2d{ "box-2D" };
inline constexpr QLatin1StringView Cst_MediaScreenShot{ "ss" };
inline constexpr QLatin1StringView Cst_MediaSsTitle{ "sstitle" };
inline constexpr QLatin1StringView Cst_MediaBox3d{ "box-3D" };
inline constexpr QLatin1StringView Cst_MediaMix1{ "mixrbv1" };
inline constexpr QLatin1StringView Cst_MediaMix2{ "mixrbv2" };
inline constexpr QLatin1StringView Cst_MediaArcadeBox1{ "ssarcademyboxv1" };
inline constexpr QLatin1StringView Cst_MediaWheel{ "wheel" };
inline constexpr QLatin1StringView Cst_MediaVideo{ "video" };
