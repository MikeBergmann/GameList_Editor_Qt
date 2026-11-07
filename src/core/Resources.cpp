#include "Resources.h"

#include <iterator>

// clang-format off
constexpr SystemInfo Cst_Systems[] = {
   { skNES,          "Nintendo",                  "nes",           "nes.png",           "3",   "Nintendo Entertainment System"       },
   { skSNES,         "Super Nintendo",            "snes",          "snes.png",          "4",   "Super Nintendo Entertainment System" },
   { skMasterSystem, "Master System",             "mastersystem",  "mastersystem.png",  "2",   "Sega Master System"                  },
   { skMegaDrive,    "MegaDrive",                 "megadrive",     "megadrive.png",     "1",   "Sega Genesis"                        },
   { skNeoGeo,       "Neo Geo",                   "neogeo",        "neogeo.png",        "75",  "SNK Neo Geo AES"                     },
   { skCPC,          "Amstrad CPC",               "amstradcpc",    "amstradcpc.png",    "65",  "Amstrad CPC"                         },
   { skAT2600,       "Atari 2600",                "atari2600",     "atari2600.png",     "26",  "Atari 2600"                          },
   { skAT7800,       "Atari 7800",                "atari7800",     "atari7800.png",     "41",  "Atari 7800"                          },
   { skATST,         "Atari ST",                  "atarist",       "atarist.png",       "42",  "Atari ST"                            },
   { skCS,           "CaveStory",                 "cavestory",     "cavestory.png",     "138", ""                                    },
   { skFCD,          "Family Computer Disk",      "fds",           "fds.png",           "106", "Nintendo Famicom Disk System"        },
   { skFBA,          "Final Burn Alpha",          "fba",           "fba.png",           "75",  "Arcade"                              },
   { skFBALib,       "Final Burn Alpha Libretro", "fba_libretro",  "fba_libretro.png",  "75",  "Arcade"                              },
   { skGW,           "Game & Watch",              "gw",            "gw.png",            "52",  "Nintendo Game & Watch"               },
   { skGBC,          "Gameboy Color",             "gbc",           "gbc.png",           "10",  "Nintendo Game Boy Color"             },
   { skGG,           "Game Gear",                 "gamegear",      "gamegear.png",      "21",  "Sega Game Gear"                      },
   { skGB,           "Gameboy",                   "gb",            "gb.png",            "9",   "Nintendo Game Boy"                   },
   { skGBA,          "Gameboy Advance",           "gba",           "gba.png",           "12",  "Nintendo Game Boy Advance"           },
   { skLU,           "Lutro",                     "lutro",         "lutro.png",         "75",  ""                                    },
   { skLYNX,         "Lynx",                      "lynx",          "lynx.png",          "28",  "Atari Lynx"                          },
   { skMAME,         "Mame",                      "mame",          "mame.png",          "75",  "Arcade"                              },
   { skMSX,          "MSX 1-2-2+",                "msx",           "msx.png",           "113", "Microsoft MSX"                       },
   { skMSX1,         "MSX 1",                     "msx1",          "msx1.png",          "113", "Microsoft MSX"                       },
   { skMSX2,         "MSX 2+",                    "msx2",          "msx2.png",          "113", "Microsoft MSX2"                      },
   { skNGP,          "Neo Geo Pocket B&W",        "ngp",           "ngp.png",           "25",  "SNK Neo Geo Pocket"                  },
   { skNGPC,         "Neo Geo Pocket Color",      "ngpc",          "ngpc.png",          "82",  "SNK Neo Geo Pocket Color"            },
   { skN64,          "Nintendo 64",               "n64",           "n64.png",           "14",  "Nintendo 64"                         },
   { skODY,          "Odyssey 2",                 "o2em",          "o2em.png",          "104", "Magnavox Odyssey 2"                  },
   { skPCE,          "PC Engine",                 "pcengine",      "pcengine.png",      "31",  "NEC TurboGrafx-16"                   },
   { skPCECD,        "PC Engine CD",              "pcenginecd",    "pcenginecd.png",    "114", "NEC TurboGrafx-CD"                   },
   { skPS,           "Playstation",               "psx",           "psx.png",           "57",  "Sony Playstation"                    },
   { skPRB,          "PR Boom",                   "prboom",        "prboom.png",        "135", ""                                    },
   { skSVM,          "Scumm VM",                  "scummvm",       "scummvm.png",       "123", "ScummVM"                             },
   { skS32X,         "Sega 32X",                  "sega32x",       "sega32x.png",       "19",  "Sega 32X"                            },
   { skSCD,          "Sega CD",                   "segacd",        "segacd.png",        "20",  "Sega CD"                             },
   { skSG1000,       "Sega SG 1000",              "sg1000",        "sg1000.png",        "109", "Sega SG-1000"                        },
   { skSGFX,         "Supergrafx",                "supergrafx",    "supergrafx.png",    "105", "PC Engine SuperGrafx"                },
   { skVCX,          "Vectrex",                   "vectrex",       "vectrex.png",       "102", "GCE Vectrex"                         },
   { skVB,           "Virtual Boy",               "virtualboy",    "virtualboy.png",    "11",  "Nintendo Virtual Boy"                },
   { skWS,           "Wonderswan B&W",            "wswan",         "wswan.png",         "45",  "WonderSwan"                          },
   { skWSC,          "Wonderswan Color",          "wswanc",        "wswanc.png",        "46",  "WonderSwan Color"                    },
   { skZXS,          "ZX Spectrum",               "zxspectrum",    "zxspectrum.png",    "76",  "Sinclair ZX Spectrum"                },
   { skZX81,         "ZX81",                      "zx81",          "zx81.png",          "77",  "Sinclair ZX-81"                      },
   { skAM1200,       "Amiga 1200",                "amiga1200",     "amiga1200.png",     "64",  "Commodore Amiga"                     },
   { skAM600,        "Amiga 600",                 "amiga600",      "amiga600.png",      "64",  "Commodore Amiga"                     },
   { skAPPLE,        "Apple II",                  "apple2",        "apple2.png",        "86",  "Apple II"                            },
   { skCV,           "Colecovision",              "colecovision",  "colecovision.png",  "48",  "ColecoVision"                        },
   { skC64,          "Commodore 64",              "c64",           "c64.png",           "66",  "Commodore 64"                        },
   { skDB,           "DosBox",                    "dos",           "dos.png",           "135", "MS-DOS"                              },
   { skDC,           "Dreamcast",                 "dreamcast",     "dreamcast.png",     "23",  "Sega Dreamcast"                      },
   { skGC,           "Gamecube",                  "gc",            "gc.png",            "13",  "Nintendo GameCube"                   },
   { skPSP,          "Playstation Portable",      "psp",           "psp.png",           "61",  "Sony PSP"                            },
   { skWII,          "Wii",                       "wii",           "wii.png",           "16",  "Nintendo Wii"                        },
   { skGenesis,      "Genesis",                   "genesis",       "genesis.png",       "1",   "Sega Genesis"                        },
   { sk3do,          "3DO",                       "3do",           "3do.png",           "29",  "3DO Interactive Multiplayer"         },
   { skAM,           "Amiga",                     "amiga",         "amiga.png",         "64",  "Commodore Amiga"                     },
   { skAMCD,         "Amiga CD",                  "amigacd32",     "amigacd32.png",     "130", "Commodore Amiga CD32"                },
   { skARC,          "Arcade",                    "arcade",        "arcade.png",        "75",  "Arcade"                              },
   { skAT5200,       "Atari 5200",                "atari5200",     "atari5200.png",     "40",  "Atari 5200"                          },
   { skATLX,         "Atari Lynx",                "atarilynx",     "lynx.png",          "28",  "Atari Lynx"                          },
   { skCDTV,         "CDTV",                      "cdtv",          "cdtv.png",          "129", "Commodore CDTV"                      },
   { skCV2,          "Colecovision",              "coleco",        "colecovision.png",  "48",  "ColecoVision"                        },
   { skDaphne,       "Daphne",                    "daphne",        "daphne.png",        "49",  ""                                    },
   { skGX4000,       "Amstrad GX4000",            "gx4000",        "gx4000.png",        "87",  "Amstrad GX4000"                      },
   { skIV,           "Intellivision",             "intellivision", "intellivision.png", "115", "Mattel Intellivision"                },
   { skNaomi,        "Naomi",                     "naomi",         "naomi.png",         "56",  "Sega Naomi"                          },
   { skNGCD,         "NeoGeo CD",                 "neogeocd",      "neogeocd.png",      "70",  "SNK Neo Geo CD"                      },
   { skOB,           "OpenBor",                   "openbor",       "openbor.png",       "214", "OpenBOR"                             },
   { skPorts,        "Ports",                     "ports",         "ports.png",         "135", ""                                    },
   { skSS,           "Sega Saturn",               "saturn",        "saturn.png",        "22",  "Sega Saturn"                         },
   { skSNESCD,       "Super Nintendo CD",         "snescd",        "snescd.png",        "210", ""                                    },
   { skX68000,       "Sharp X68000",              "x68000",        "x68000.png",        "79",  "Sharp X68000"                        },
   { skAW,           "Atomiswave",                "atomiswave",    "atomiswave.png",    "53",  "Sammy Atomiswave"                    },
   { skTI99,         "TI-99",                     "ti99",          "ti99.png",          "205", "Texas Instruments TI 99/4A"          },
   { skMSU1,         "Super Nintendo MSU1",       "snesmsu1",      "snesmsu1.png",      "210", "Super Nintendo Entertainment System" },
   { skGW2,          "Game & Watch",              "gameandwatch",  "gw.png",            "52",  "Nintendo Game & Watch"               },
   { skNDS,          "Nintendo DS",               "nds",           "nds.png",           "15",  "Nintendo DS"                         },
   { skODY2,         "Odyssey 2",                 "odyssey2",      "o2em.png",          "104", "Magnavox Odyssey 2"                  },
   { skTG,           "PC Engine",                 "tg16",          "pcengine.png",      "31",  "NEC TurboGrafx-16"                   },
   { skTGCD,         "PC Engine CD",              "tg16cd",        "pcenginecd.png",    "114", "NEC TurboGrafx-CD"                   },
   { skPSPM,         "PSP Minis",                 "pspminis",      "pspminis.png",      "172", "Sony PSP Minis"                      },
   { skWindows,      "Windows",                   "windows",       "other.png",         "138", "Windows"                             },
   { skAndroid,      "Android",                   "android",       "other.png",         "63",  "Android"                             },
   { skOther,        "Other",                     "",              "other.png",         "0",   ""                                    },
};
// clang-format on

constexpr bool systemsInOrder()
{
   for ( int i = 0; i < skCount; ++i ) {
      if ( Cst_Systems[i].kind != i )
         return false;
   }
   return true;
}

static_assert( std::size( Cst_Systems ) == skCount, "one row per SystemKind" );
static_assert( systemsInOrder(), "rows must follow the SystemKind enum order" );

const SystemInfo& systemInfo( SystemKind aKind )
{
   return Cst_Systems[aKind];
}

// clang-format off
const char* const Cst_CountryName[cnCount] = {
   "", "de", "asi", "au", "br", "bg", "ca", "cl", "cn", "ame",
   "kr", "cus", "dk", "sp", "eu", "fi", "fr", "gr", "hu", "il",
   "it", "jp", "kw", "wor", "mor", "no", "nz", "oce", "nl", "pe",
   "pl", "pt", "cz", "uk", "ru", "sk", "se", "tw", "tr", "us",
};
// clang-format on

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
   { "Continent Américain", "Amerikanischen Kontinent", "American continent",
     "Continente americano", "Continente americano" },
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
   { "Monde", "Welt", "World", "Mundo", "Mundo" },
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
   { "Suède", "Schweden", "Sweden", "Suecia", "Suécia" },
   { "Taiwan", "Taiwan", "Taiwan", "Taiwan", "Taiwan" },
   { "Turquie", "Türkei", "Turkey", "Turquía", "Turquia" },
   { "USA", "USA", "USA", "EUA", "EUA" },
};

const char* const Cst_LangNameStr[lnCount] = { "fr", "de", "en", "es", "pt_BR" };

SystemKind systemKindFromFolder( const QString& aFolderName )
{
   for ( int kind = 0; kind < skCount; ++kind ) {
      if ( aFolderName == QLatin1String( Cst_Systems[kind].folder ) )
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
   return ( aNumber >= 0 && aNumber < lnCount ) ? static_cast<LangName>( aNumber ) : lnEnglish;
}
