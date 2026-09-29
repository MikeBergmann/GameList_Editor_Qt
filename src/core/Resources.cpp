#include "Resources.h"

const char* const Cst_SystemKindStr[skCount] = {
   "Nintendo",                    // skNES
   "Super Nintendo",              // skSNES
   "Master System",               // skMasterSystem
   "MegaDrive",                   // skMegaDrive
   "Neo Geo",                     // skNeoGeo
   "Amstrad CPC",                 // skCPC
   "Atari 2600",                  // skAT2600
   "Atari 7800",                  // skAT7800
   "Atari ST",                    // skATST
   "CaveStory",                   // skCS
   "Family Computer Disk",        // skFCD
   "Final Burn Alpha",            // skFBA
   "Final Burn Alpha Libretro",   // skFBALib
   "Game & Watch",                // skGW
   "Gameboy Color",               // skGBC
   "Game Gear",                   // skGG
   "Gameboy",                     // skGB
   "Gameboy Advance",             // skGBA
   "Lutro",                       // skLU
   "Lynx",                        // skLYNX
   "Mame",                        // skMAME
   "MSX 1-2-2+",                  // skMSX
   "MSX 1",                       // skMSX1
   "MSX 2+",                      // skMSX2
   "Neo Geo Pocket B&W",          // skNGP
   "Neo Geo Pocket Color",        // skNGPC
   "Nintendo 64",                 // skN64
   "Odyssey 2",                   // skODY
   "PC Engine",                   // skPCE
   "PC Engine CD",                // skPCECD
   "Playstation",                 // skPS
   "PR Boom",                     // skPRB
   "Scumm VM",                    // skSVM
   "Sega 32X",                    // skS32X
   "Sega CD",                     // skSCD
   "Sega SG 1000",                // skSG1000
   "Supergrafx",                  // skSGFX
   "Vectrex",                     // skVCX
   "Virtual Boy",                 // skVB
   "Wonderswan B&W",              // skWS
   "Wonderswan Color",            // skWSC
   "ZX Spectrum",                 // skZXS
   "ZX81",                        // skZX81
   "Amiga 1200",                  // skAM1200
   "Amiga 600",                   // skAM600
   "Apple II",                    // skAPPLE
   "Colecovision",                // skCV
   "Commodore 64",                // skC64
   "DosBox",                      // skDB
   "Dreamcast",                   // skDC
   "Gamecube",                    // skGC
   "Playstation Portable",        // skPSP
   "Wii",                         // skWII
   "Genesis",                     // skGenesis
   "3DO",                         // sk3do
   "Amiga",                       // skAM
   "Amiga CD",                    // skAMCD
   "Arcade",                      // skARC
   "Atari 5200",                  // skAT5200
   "Atari Lynx",                  // skATLX
   "CDTV",                        // skCDTV
   "Colecovision",                // skCV2
   "Daphne",                      // skDaphne
   "Amstrad GX4000",              // skGX4000
   "Intellivision",               // skIV
   "Naomi",                       // skNaomi
   "NeoGeo CD",                   // skNGCD
   "OpenBor",                     // skOB
   "Ports",                       // skPorts
   "Sega Saturn",                 // skSS
   "Super Nintendo CD",           // skSNESCD
   "Sharp X68000",                // skX68000
   "Atomiswave",                  // skAW
   "TI-99",                       // skTI99
   "Super Nintendo MSU1",         // skMSU1
   "Game & Watch",                // skGW2
   "Nintendo DS",                 // skNDS
   "Odyssey 2",                   // skODY2
   "PC Engine",                   // skTG
   "PC Engine CD",                // skTGCD
   "PSP Minis",                   // skPSPM
   "Other",                       // skOther
};

const char* const Cst_SystemKindFolderNames[skCount] = {
   "nes",                         // skNES
   "snes",                        // skSNES
   "mastersystem",                // skMasterSystem
   "megadrive",                   // skMegaDrive
   "neogeo",                      // skNeoGeo
   "amstradcpc",                  // skCPC
   "atari2600",                   // skAT2600
   "atari7800",                   // skAT7800
   "atarist",                     // skATST
   "cavestory",                   // skCS
   "fds",                         // skFCD
   "fba",                         // skFBA
   "fba_libretro",                // skFBALib
   "gw",                          // skGW
   "gbc",                         // skGBC
   "gamegear",                    // skGG
   "gb",                          // skGB
   "gba",                         // skGBA
   "lutro",                       // skLU
   "lynx",                        // skLYNX
   "mame",                        // skMAME
   "msx",                         // skMSX
   "msx1",                        // skMSX1
   "msx2",                        // skMSX2
   "ngp",                         // skNGP
   "ngpc",                        // skNGPC
   "n64",                         // skN64
   "o2em",                        // skODY
   "pcengine",                    // skPCE
   "pcenginecd",                  // skPCECD
   "psx",                         // skPS
   "prboom",                      // skPRB
   "scummvm",                     // skSVM
   "sega32x",                     // skS32X
   "segacd",                      // skSCD
   "sg1000",                      // skSG1000
   "supergrafx",                  // skSGFX
   "vectrex",                     // skVCX
   "virtualboy",                  // skVB
   "wswan",                       // skWS
   "wswanc",                      // skWSC
   "zxspectrum",                  // skZXS
   "zx81",                        // skZX81
   "amiga1200",                   // skAM1200
   "amiga600",                    // skAM600
   "apple2",                      // skAPPLE
   "colecovision",                // skCV
   "c64",                         // skC64
   "dos",                         // skDB
   "dreamcast",                   // skDC
   "gc",                          // skGC
   "psp",                         // skPSP
   "wii",                         // skWII
   "genesis",                     // skGenesis
   "3do",                         // sk3do
   "amiga",                       // skAM
   "amigacd32",                   // skAMCD
   "arcade",                      // skARC
   "atari5200",                   // skAT5200
   "atarilynx",                   // skATLX
   "cdtv",                        // skCDTV
   "coleco",                      // skCV2
   "daphne",                      // skDaphne
   "gx4000",                      // skGX4000
   "intellivision",               // skIV
   "naomi",                       // skNaomi
   "neogeocd",                    // skNGCD
   "openbor",                     // skOB
   "ports",                       // skPorts
   "saturn",                      // skSS
   "snescd",                      // skSNESCD
   "x68000",                      // skX68000
   "atomiswave",                  // skAW
   "ti99",                        // skTI99
   "snesmsu1",                    // skMSU1
   "gameandwatch",                // skGW2
   "nds",                         // skNDS
   "odyssey2",                    // skODY2
   "tg16",                        // skTG
   "tg16cd",                      // skTGCD
   "pspminis",                    // skPSPM
   "",                            // skOther
};

const char* const Cst_SystemKindImageNames[skCount] = {
   "nes.png",                     // skNES
   "snes.png",                    // skSNES
   "mastersystem.png",            // skMasterSystem
   "megadrive.png",               // skMegaDrive
   "neogeo.png",                  // skNeoGeo
   "amstradcpc.png",              // skCPC
   "atari2600.png",               // skAT2600
   "atari7800.png",               // skAT7800
   "atarist.png",                 // skATST
   "cavestory.png",               // skCS
   "fds.png",                     // skFCD
   "fba.png",                     // skFBA
   "fba_libretro.png",            // skFBALib
   "gw.png",                      // skGW
   "gbc.png",                     // skGBC
   "gamegear.png",                // skGG
   "gb.png",                      // skGB
   "gba.png",                     // skGBA
   "lutro.png",                   // skLU
   "lynx.png",                    // skLYNX
   "mame.png",                    // skMAME
   "msx.png",                     // skMSX
   "msx1.png",                    // skMSX1
   "msx2.png",                    // skMSX2
   "ngp.png",                     // skNGP
   "ngpc.png",                    // skNGPC
   "n64.png",                     // skN64
   "o2em.png",                    // skODY
   "pcengine.png",                // skPCE
   "pcenginecd.png",              // skPCECD
   "psx.png",                     // skPS
   "prboom.png",                  // skPRB
   "scummvm.png",                 // skSVM
   "sega32x.png",                 // skS32X
   "segacd.png",                  // skSCD
   "sg1000.png",                  // skSG1000
   "supergrafx.png",              // skSGFX
   "vectrex.png",                 // skVCX
   "virtualboy.png",              // skVB
   "wswan.png",                   // skWS
   "wswanc.png",                  // skWSC
   "zxspectrum.png",              // skZXS
   "zx81.png",                    // skZX81
   "amiga1200.png",               // skAM1200
   "amiga600.png",                // skAM600
   "apple2.png",                  // skAPPLE
   "colecovision.png",            // skCV
   "c64.png",                     // skC64
   "dos.png",                     // skDB
   "dreamcast.png",               // skDC
   "gc.png",                      // skGC
   "psp.png",                     // skPSP
   "wii.png",                     // skWII
   "genesis.png",                 // skGenesis
   "3do.png",                     // sk3do
   "amiga.png",                   // skAM
   "amigacd32.png",               // skAMCD
   "arcade.png",                  // skARC
   "atari5200.png",               // skAT5200
   "lynx.png",                    // skATLX
   "cdtv.png",                    // skCDTV
   "colecovision.png",            // skCV2
   "daphne.png",                  // skDaphne
   "gx4000.png",                  // skGX4000
   "intellivision.png",           // skIV
   "naomi.png",                   // skNaomi
   "neogeocd.png",                // skNGCD
   "openbor.png",                 // skOB
   "ports.png",                   // skPorts
   "saturn.png",                  // skSS
   "snescd.png",                  // skSNESCD
   "x68000.png",                  // skX68000
   "atomiswave.png",              // skAW
   "ti99.png",                    // skTI99
   "snesmsu1.png",                // skMSU1
   "gw.png",                      // skGW2
   "nds.png",                     // skNDS
   "o2em.png",                    // skODY2
   "pcengine.png",                // skTG
   "pcenginecd.png",              // skTGCD
   "pspminis.png",                // skPSPM
   "other.png",                   // skOther
};


// Screenscraper.fr system IDs, retrieved with this URL:
// https://www.screenscraper.fr/api2/systemesListe.php?devid=xxx&devpassword=yyy&softname=zzz&output=XML&ssid=test&sspassword=test
const char* const Cst_SystemKindId[skCount] = {
   "3", "4", "2", "1", "75", "65", "26", "41", "42", "138", "106", "75",
   "75", "52", "10", "21", "9", "12", "75", "28", "75", "113", "113", "113",
   "25", "82", "14", "104", "31", "114", "57", "135", "123", "19", "20", "109",
   "105", "102", "11", "45", "46", "76", "77", "64", "64", "86", "48", "66",
   "135", "23", "13", "61", "16", "1", "29", "64", "130", "75", "40", "28",
   "129", "48", "49", "87", "115", "56", "70", "214", "135", "22", "210", "79",
   "53", "205", "210", "52", "15", "104", "31", "114", "172", "0",
};

const char* const Cst_CountryName[cnCount] = {
   "", "de", "asi", "au", "br", "bg", "ca", "cl", "cn", "ame",
   "kr", "cus", "dk", "sp", "eu", "fi", "fr", "gr", "hu", "il",
   "it", "jp", "kw", "wor", "mor", "no", "nz", "oce", "nl", "pe",
   "pl", "pt", "cz", "uk", "ru", "sk", "se", "tw", "tr", "us",
};

const char* const Cst_CountryNameFull[cnCount][lnCount] = {
   { "", "", "", "", "" },
   { "Allemagne", "Deutschland", "Germany", "Alemania", "Alemanha" },
   { "Asie", "Asien", "Asia", "Asia", "Ásia" },
   { "Australie", "Australien", "Australia", "Australia", "Austrália" },
   { "Brésil", "Brasilien", "Brazil", "Brasil", "Brasil" },
   { "Bulgarie", "Bulgarien", "Bulgaria", "Bulgaria", "Bulgária" },
   { "Canada", "Kanada", "Canada", "Canadá", "Canadá" },
   { "Chili", "Chile", "Chile", "Chile", "Chile" },
   { "Chine", "China", "China", "China", "China" },
   { "Continent Américain", "Amerikanischen Kontinent", "American continent", "Continente americano", "Continente americano" },
   { "Corée", "Korea", "Korea", "Corea", "Coreia" },
   { "Custom", "Maßgeschneidert", "Custom", "Personalizado", "Personalizadas" },
   { "Danemark", "Dänemark", "Denmark", "Dinamarca", "Dinamarca" },
   { "Espagne", "Spanien", "Spain", "España", "Espanha" },
   { "Europe", "Europa", "Europe", "Europa", "Europa" },
   { "Finlande", "Finnland", "Finland", "Finlandia", "Finlândia" },
   { "France", "Frankreich", "France", "Francia", "França" },
   { "Grèce", "Griechenland", "Greece", "Grecia", "Grécia" },
   { "Hongrie", "Ungarn", "Hungary", "Hungría", "Hungria" },
   { "Israel", "Israel", "Israel", "Israel", "Israel" },
   { "Italie", "Italien", "Italy", "Italia", "Itália" },
   { "Japon", "Japan", "Japan", "Japón", "Japão" },
   { "Koweït", "Kuwait", "Kuwait", "Kuwait", "Kuweit" },
   { "Monde", "World", "World", "Mundo", "Mundo" },
   { "Moyen-Orient", "Naher Osten", "Middle East", "Medio Oriente", "Médio Oriente" },
   { "Norvège", "Norwegen", "Norway", "Noruega", "Noruega" },
   { "Nouvelle-Zélande", "Neuseeland", "New Zealand", "Nueva Zelanda", "Nova Zelândia" },
   { "Océanie", "Ozeanien", "Oceania", "Oceanía", "Oceânia" },
   { "Pays-Bas", "Niederlande", "Netherlands", "Holanda", "Holanda" },
   { "Pérou", "Peru", "Peru", "Perú", "Peru" },
   { "Pologne", "Polen", "Poland", "Polonia", "Polônia" },
   { "Portugal", "Portugal", "Portugal", "Portugal", "Portugal" },
   { "République Tchèque", "Tschechien", "Czech republic", "República Checa", "República Checa" },
   { "Royaume-Uni", "Großbritannien", "United Kingdom", "Reino Unido", "Reino Unido" },
   { "Russie", "Russland", "Russia", "Rusia", "Rússia" },
   { "Slovaquie", "Slowakei", "Slovakia", "Eslovaquia", "Eslováquia" },
   { "Suede", "Schweden", "Sweden", "Suecia", "Suécia" },
   { "Taiwan", "Taiwan", "Taiwan", "Taiwan", "Taiwan" },
   { "Turquie", "Türkei", "Turkey", "Turquía", "Turquia" },
   { "USA", "USA", "USA", "EUA", "EUA" },
};

const char* const Cst_LangNameStr[lnCount] = {
   "fr", "de", "en", "es", "pt_BR"
};

SystemKind systemKindFromFolder( const QString& aFolderName )
{
   for ( int kind = 0; kind < skCount; ++kind ) {
      if ( aFolderName == QLatin1String( Cst_SystemKindFolderNames[kind] ) )
         return static_cast<SystemKind>( kind );
   }

   return skOther;
}

CountryName countryFromShortName( const QString& aShortName )
{
   for ( int country = 0; country < cnCount; ++country ) {
      if ( aShortName == QLatin1String( Cst_CountryName[country] ) )
         return static_cast<CountryName>( country );
   }

   return cnUnd;
}

LangName langFromIndex( int aNumber )
{
   return ( aNumber >= 0 && aNumber < lnCount ) ? static_cast<LangName>( aNumber )
                                                : lnEnglish;
}
