# GameList_Editor_Qt

## The Why

**Qt6 Port of GameList Editor Delphi**

There are a lot of great ROMs for vintage computers/consoles or for their emulators on itch.io.

To manage these ROMs, I use GameList Editor (https://github.com/andresdelcampo/GameList_Editor) as my 'the' tool for editing/scaping my gamelist.xml.

I want to add some features, but unfortunately, it is written in Delphi (which I don't have a dev toolchain for).

So this repo will hopefully become a C++/Qt6 port.

## First Step

- Target: Linux Mint 22.1 (Ubuntu 24.04), Qt6.4, cmake, gcc
- Testing: Google Test
- First drop will contain:
  - No video playback
  - No SSH / no Pi remote control
  - English-only UI
  - No theming (default Qt widget style only)

## Features to add

These are the features I want to implement (and therefore do all this):

- Additional Scrapers (e.g., Launchbox)
- Specific handling of the xml/media folders, e.g like es-de:
```
├── gamelists/
│   └── nes/
│       └── gamelist.xml
└── downloaded_media/
    └── nes/
        ├── covers/
        ├── screenshots/
        ├── videos/
  ```
- Normalize media. E.g., naming, size, etc.
- Remove unused media

## Still undecided

Whether Windows stays a supported target or this is Linux-only. Unfortunately, my Mac no longer runs macOS, so there will be no macOS version.

