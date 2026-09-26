#pragma once

#include <QString>

// Lookup tables ported from U_Resources.pas.
//
// Names are kept verbatim - Cst_ for the tables and constants, sk/ln/cn for the
// enumerators - so that porting F_Main stays a mechanical 1:1 map.
//
// The UI resourcestrings deliberately did *not* come along, apart from the 82
// system display names below, which Cst_SystemKindStr was built out of. With
// gettext gone the rest are plain literals with one use site each, so they land
// at that site when their form is ported.

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

// const char*, not QString: Qt6 decodes those as UTF-8, which the accented
// entries of Cst_CountryNameFull need, and they cost no static construction.

// Display names for Cbx_Systems.
extern const char* const Cst_SystemKindStr[skCount];
// Names of the per-system folders under the ROM root. Empty for skOther.
extern const char* const Cst_SystemKindFolderNames[skCount];
// File names under Cst_LogoPicsFolder. Not unique: several kinds share a logo.
extern const char* const Cst_SystemKindImageNames[skCount];
// screenscraper.fr system ids. Not unique either.
extern const char* const Cst_SystemKindId[skCount];

// Two-letter codes as screenscraper.fr writes them. Cst_CountryName[cnUnd] is "".
extern const char* const Cst_CountryName[cnCount];
// Country name per UI language. Indexed [country][lang] - 0-based, so the
// Succ(FLanguage) the Delphi needed for its [1..5] array is gone.
extern const char* const Cst_CountryNameFull[cnCount][lnCount];

// Language codes, as sent to screenscraper.fr and matched against its replies.
extern const char* const Cst_LangNameStr[lnCount];

// Reverse lookups. Each falls back to the catch-all rather than failing: an
// unrecognised folder is just a system this build does not know about.
SystemKind systemKindFromFolder( const QString& aFolderName );
CountryName countryFromShortName( const QString& aShortName );
LangName langFromIndex( int aNumber );

// --- gamelist.xml: node and attribute names -------------------------------

inline constexpr const char* Cst_GameListFileName = "gamelist.xml";
inline constexpr const char* Cst_Game = "game";
inline constexpr const char* Cst_Path = "path";
inline constexpr const char* Cst_Name = "name";
inline constexpr const char* Cst_Description = "desc";
inline constexpr const char* Cst_ImageLink = "image";
inline constexpr const char* Cst_VideoLink = "video";
inline constexpr const char* Cst_Rating = "rating";
inline constexpr const char* Cst_ReleaseDate = "releasedate";
inline constexpr const char* Cst_Developer = "developer";
inline constexpr const char* Cst_Publisher = "publisher";
inline constexpr const char* Cst_Genre = "genre";
inline constexpr const char* Cst_Players = "players";
inline constexpr const char* Cst_Region = "region";
inline constexpr const char* Cst_Playcount = "playcount";
inline constexpr const char* Cst_LastPlayed = "lastplayed";
inline constexpr const char* Cst_KidGame = "kidgame";
inline constexpr const char* Cst_Hidden = "hidden";
inline constexpr const char* Cst_Favorite = "favorite";
inline constexpr const char* Cst_True = "true";
inline constexpr const char* Cst_False = "false";

// <releasedate> is yyyymmddT000000; short fields are padded out to these.
inline constexpr const char* Cst_DateShortFill = "00";
inline constexpr const char* Cst_DateLongFill = "0000";
inline constexpr const char* Cst_DateSuffix = "T000000";

inline constexpr const char* Cst_ImageSuffixPng = ".png";
inline constexpr const char* Cst_VideoSuffixMp4 = ".mp4";
inline constexpr const char* Cst_TxtExtension = ".txt";

// --- bundled assets -------------------------------------------------------

// .qrc prefixes, not filesystem paths - see Resources/assets.qrc. QPixmap reads
// ":/" transparently, POSIX calls do not.
inline constexpr const char* Cst_DefaultPicsFolderPath = ":/DefaultPictures/";
inline constexpr const char* Cst_LogoPicsFolder = ":/SystemsLogos/";
inline constexpr const char* Cst_DefaultImageName = "default.png";

// --- QSettings: group and the 13 keys -------------------------------------

inline constexpr const char* Cst_IniOptions = "Options";
inline constexpr const char* Cst_IniGodMode = "GodMode";
inline constexpr const char* Cst_IniAutoHash = "AutoHash";
inline constexpr const char* Cst_IniDelWoPrompt = "DelWoPrompt";
inline constexpr const char* Cst_ShowTips = "ShowTips";
inline constexpr const char* Cst_IniGenesisLogo = "GenesisLogo";
inline constexpr const char* Cst_IniLanguage = "Language";
inline constexpr const char* Cst_IniSSUser = "SSUser";
inline constexpr const char* Cst_IniSSPwd = "SSPwd";
inline constexpr const char* Cst_IniProxyUser = "ProxyUser";
inline constexpr const char* Cst_IniProxyPwd = "ProxyPwd";
inline constexpr const char* Cst_IniProxyServer = "ProxyServer";
inline constexpr const char* Cst_IniProxyPort = "ProxyPort";
inline constexpr const char* Cst_IniProxyUse = "ProxyUse";

// Language is the one key whose default is not the zero value: TLangName counts
// from French, and the ini this replaces always shipped Language=2 (English).
inline constexpr int Cst_IniLanguageDefault = lnEnglish;

// --- screenscraper.fr -----------------------------------------------------

inline constexpr const char* Cst_ScraperAddress = "https://www.screenscraper.fr/api2/";
inline constexpr const char* Cst_Category = "jeuInfos.php";
inline constexpr const char* Cst_ScrapeLogin = "?devid=";
inline constexpr const char* Cst_ScrapePwd = "&devpassword=";
inline constexpr const char* Cst_ScrapePwdSafe = "&devpassword=";
inline constexpr const char* Cst_DevSoftName = "&softname=GameListEditorv1";
inline constexpr const char* Cst_Output = "&output=xml";
inline constexpr const char* Cst_SSId = "&ssid=";
inline constexpr const char* Cst_SSPwd = "&sspassword=";
inline constexpr const char* Cst_Crc = "&crc=";
inline constexpr const char* Cst_SystemId = "&systemid=";
inline constexpr const char* Cst_RomName = "&romnom=";
inline constexpr const char* Cst_RomSize = "&romtaille=";
inline constexpr const char* Cst_TempXml = "temp.xml";

// Reply nodes. French, because the API is.
inline constexpr const char* Cst_DataNode = "Data";
inline constexpr const char* Cst_GameNode = "jeu";
inline constexpr const char* Cst_UserNode = "ssuser";
inline constexpr const char* Cst_ThreadNode = "maxthreads";
inline constexpr const char* Cst_MediaNode = "medias";
inline constexpr const char* Cst_NamesNode = "noms";
inline constexpr const char* Cst_RegionsNode = "regions";
inline constexpr const char* Cst_EditNode = "editeur";
inline constexpr const char* Cst_DevNode = "developpeur";
inline constexpr const char* Cst_NoteNode = "note";
inline constexpr const char* Cst_PlayersNode = "joueurs";
inline constexpr const char* Cst_SynopNode = "synopsis";
inline constexpr const char* Cst_DateNode = "dates";
inline constexpr const char* Cst_GenreNode = "genres";
inline constexpr const char* Cst_AttType = "type";
inline constexpr const char* Cst_AttFormat = "format";
inline constexpr const char* Cst_AttRegion = "region";
inline constexpr const char* Cst_AttLang = "langue";
inline constexpr const char* Cst_AttId = "id";
inline constexpr const char* Cst_PngExt = "png";

// <media type="..."> values the scraper offers, in the order Scl_Pictures shows.
inline constexpr const char* Cst_MediaBox2d = "box-2D";
inline constexpr const char* Cst_MediaScreenShot = "ss";
inline constexpr const char* Cst_MediaSsTitle = "sstitle";
inline constexpr const char* Cst_MediaBox3d = "box-3D";
inline constexpr const char* Cst_MediaMix1 = "mixrbv1";
inline constexpr const char* Cst_MediaMix2 = "mixrbv2";
inline constexpr const char* Cst_MediaArcadeBox1 = "ssarcademyboxv1";
inline constexpr const char* Cst_MediaWheel = "wheel";
inline constexpr const char* Cst_MediaVideo = "video";
