# GameList_Editor_Qt

## The Why

**Qt6 Port of GameList Editor Delphi**

There are a lot of great ROMs for vintage computers/consoles or for their emulators on itch.io.

To manage these ROMs, I use [GameList Editor](https://github.com/andresdelcampo/GameList_Editor) as my 'the' tool for editing/scaping my gamelist.xml.

I want to add some features, but unfortunately, it is written in Delphi (which I don't have a dev toolchain for).

So this repo will hopefully become a C++/Qt6 port of the GPLv3 `andresdelcampo/GameList_Editor` (itself a fork of `NeeeeB/GameList_Editor`).

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

## AI usage

Yes, I used and will use AI. In my opinion, AI is a great tool for every developer to speed up their work and keep learning, especially if you have no colleagues, as in side projects like this.

So I used AI for:

- Translating the original French comments to English
- Checking the grammar and spelling of my texts, because I'm not a native English speaker.
- Helping me analyze the Delphi codebase, because the last time I programmed with Delphi was in 1996.
- Generating all the unit-tests and test-data. Doing that manually is just tedious and nothing I do for fun in a side project.
- Writing new code, based on my design and instructions. 
- Converting resource tables from Delphi to C++/Qt6.
- Doing code review and refactoring, in the absence of a human reviewer.

This list is not final, and I will probably find more ways to use AI as I go along. Nevertheless, I will not use AI without steering and reviewing the result. I do this for fun and want to learn and understand.
