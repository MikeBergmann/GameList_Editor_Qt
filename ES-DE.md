# ES-DE layout support (and later sync)

Plan only. Phases are meant to land one at a time, each reviewable on its own.

## 1. The core difference

| | Legacy ES / Batocera / RetroBat | ES-DE |
|---|---|---|
| Gamelist | `<roms>/<sys>/gamelist.xml` | `<appdata>/gamelists/<sys>/gamelist.xml` (unless `LegacyGamelistFileLocation=true`) |
| ROMs | next to the gamelist | `ROMDirectory/<sys>` from `settings/es_settings.xml` |
| Media | `<image>`/`<video>` link, relative to the gamelist folder | no link; found by convention: `MediaDirectory/<sys>/<type>/<rom rel path minus ext>.<png\|jpg\|webp>` |
| `<path>` | relative to the gamelist folder | `./…` relative to `ROMDirectory/<sys>` |
| System names | `sg-1000`, … | `sg1000`, … |

There are two separate axes:

- **Where the gamelist lives:** next to the ROMs, or in a separate folder.
- **How media is found:** by link or by convention.

`LegacyGamelistFileLocation=true` combines "gamelist next to ROMs" with "media by convention", so
the code must not merge the two axes into one flag.

**What blocks this today:** `Gamelist::load` sets `FSystemDir` to the gamelist's folder and uses
that one folder as both the ROM base and the media base. Everything else depends on that.

```mermaid
flowchart TD
    A[SystemEntry] --> B[gamelistPath]
    A --> C[romDir]
    A --> D[mediaDir + MediaLayout]
    C --> E[romPath = resolvePath romDir + path]
    D --> F{layout}
    F -->|Linked| G[imagePath = resolvePath romDir + image tag]
    F -->|Conventional| H[probe mediaDir/type/rom.png jpg webp]
```

### ES-DE reference facts

- App data: `~/ES-DE` (Windows `C:\Users\<u>\ES-DE`, portable `ES-DE\ES-DE`; old releases
  `~/.emulationstation`).
- Settings: `<appdata>/settings/es_settings.xml`, entries like
  `<string name="ROMDirectory" value="..."/>`.
  - `ROMDirectory` defaults to `~/ROMs` (portable `ES-DE\ROMs`); may contain `~` or `%ESPATH%`.
  - `MediaDirectory` empty means `<appdata>/downloaded_media`; `%ESPATH%` allowed.
- Gamelists inside ROM folders are ignored unless `LegacyGamelistFileLocation=true`; new ones are
  always created under `gamelists/`.
- Media types: `3dboxes`, `backcovers`, `covers`, `custom`, `fanart`, `manuals`, `marquees`,
  `miximages`, `physicalmedia`, `screenshots`, `titlescreens`, `videos`.
  - Subfolders of the ROM path are mirrored under the type folder.
  - Directories interpreted as files keep their suffix: `dig.scummvm.png`.
  - Image extensions `jpg png webp`; video `mp4 mkv avi wmv mov webm m4v`; lowercase on Linux.
- Extra ES-DE fields: `sortname`, `collectionsortname`, `completed`, `broken`, `nogamecount`,
  `nomultiscrape`, `hidemetadata`, `controller`, `altemulator`, `playtime`, `screen`, plus
  `<folder>` entries.
- Compatibility with legacy ES is one-way: ES-DE may drop tags it does not know (but see the
  findings below: on the sample install it kept them).
- ES-DE keeps entries whose ROM is missing, and rewrites gamelists on exit like ES does.

### Findings from a real install (Android ES-DE on an SD card)

Card layout: `ES-DE/` (app data) next to `ROMs/<sys>/`. `ROMs/<sys>/` also holds a hand-copied
legacy `gamelist.xml` + `images/` + `videos/` for 19 of the 23 systems. ES-DE ignores those
files; they are the user's manual stand-in for the sync feature.

- **`gamelist.xml` is multi-root** when a system-wide alternative emulator is set (`mame`,
  `scummvm`):

  ```xml
  <?xml version="1.0"?>
  <alternativeEmulator>
  	<label>MAME 2003-Plus</label>
  </alternativeEmulator>
  <gameList>
  	<game>…
  ```

  `Gamelist::load` fails on it today: `setContent` rejects a second root, and
  `documentElement()` would be `<alternativeEmulator>` anyway.
- **`es_settings.xml` is multi-root** (flat `<bool …/>`, `<string …/>` lines, no wrapper).
- **No `ROMDirectory` key** on Android; `MediaDirectory` is empty (so `downloaded_media`). The
  ROMs are at `<appdata>/../ROMs`, which matches the portable default.
- **ES-DE kept the Batocera/Skraper tags** it does not use: `<image>`, `<video>` (500+ each),
  `<provider>`, `gametime`, `genreid`, `family`, `lang`, `region`, `arcadesystemname`,
  `<game id="…">`. ES-DE reformats with tabs and `<?xml version="1.0"?>`.
- **`<folder>` entries** exist (`pcengine`: multi-disc `./Avenger/…cue`). The loader only reads
  `<game>`, so they stay untouched in the DOM.
- **Media in use:** `covers` (23 systems) and `videos` (20); the other types only in 3 systems.
  Extensions: 1126 `png`, 694 `mp4`, 4 `jpg`, 1 `jpeg` (ES-DE ignores `.jpeg`), plus two `.bak`
  files and an Android `.nomedia` marker.

## 2. Phases

### Phase 0: split the folders (refactor, no behaviour change)

- **`SystemEntry` (`Systems.h`):** add `romDir` (empty means the gamelist's folder, so nothing
  changes for current users). **Done.** `mediaDir` and `enum MediaLayout { Linked, Conventional }`
  arrive with Phase 2, where something reads them.
- **`Gamelist::load`:** `FSystemDir` is now `FRomDir`, taken from `SystemEntry::romDir`. **Done.**
- **`systemDir()` callers:** all renamed to `romDir()`; `gamelistBackups` already used
  `system().gamelistPath`. **Done.**
- **Side benefit:** this also covers RetroPie's `~/.emulationstation/gamelists/<sys>`, which is
  the same split with linked media.
- **Tests:** the existing suite passes unchanged; one new test loads a gamelist whose ROMs and
  media live in another folder.

### Phase 1: detect and scan ES-DE (`Systems.cpp`) — done

- **Detection:** `scanSystems` treats a root as ES-DE when it holds `settings/es_settings.xml` and
  `gamelists/`. Any other root is scanned as before (legacy view, `romDir` empty).
- **`ROMDirectory`:** read from `es_settings.xml` (parsed under a wrapper, like the gamelist),
  `~` and `%ESPATH%` expanded. Candidates in order: the setting, then `<appdata>/../ROMs`
  (also the answer when the key is absent, as on Android). The first that exists wins, so a
  card moved to another device still works.
  - `%ESPATH%` is assumed to be the app-data folder's parent (portable layout).
  - Not done: asking the user when neither exists. Games then show as orphans.
- **Scan:** `gamelists/*/gamelist.xml` → `SystemEntry{ sys, kind, gamelist, romDir/<sys> }`.
  Systems with no gamelist are ignored in both layouts (decided).
- **Not done:** `LegacyGamelistFileLocation=true` (gamelists inside the ROM folders).
- **Seen on the sample card:** all 23 systems load; ES-DE's system names (`sg1000`, …) that the
  tables do not know land in `skOther` and can be mapped by the user as today.
- **Media folder:** `MediaDirectory` from the settings (`~` and `%ESPATH%` expanded), else
  `<appdata>/downloaded_media`; `SystemEntry::mediaDir` is `<media>/<sys>`. A non-empty `mediaDir`
  is the ES-DE layout switch (`Gamelist::conventionalMedia()`); there is no enum.

### Phase 1b: multi-root `gamelist.xml` (needed by any ES-DE load) — done

- `load` parses the file under a wrapper element (after the declaration, so a declared encoding
  still applies), finds `<gameList>` in it, then moves the wrapper's children up to the
  document and drops the wrapper. `save()` is unchanged; top-level siblings such as
  `<alternativeEmulator>` and their order survive the round trip.
- The declaration is rewritten to `<?xml version="1.0"?>`, because `save()` always writes UTF-8
  (a file declared ISO-8859-1 used to keep that declaration over UTF-8 bytes).
- A file with no `<gameList>` is now an error (it used to accept any root name).

### Phase 2: conventional media in `Gamelist` — done

- **Stem:** the ROM path cleaned (`./a/b.zip` → `a/b`), minus its last extension; subfolders are
  mirrored. A folder ROM keeps its whole name (`dig.scummvm.png`). Two ROMs with one stem share
  the media, as in ES-DE.
- **Reading:** `load()` indexes `covers/` and `videos/` once each (`QDirIterator`), keyed by stem;
  `<image>`/`<video>` are ignored in this view (`imagePath`/`videoPath` stay empty) but stay in
  the DOM. Image extensions `jpg` (preferred), `png`, `webp`; video `mp4 mkv avi wmv mov webm m4v`.
  Lowercase only on case-sensitive volumes, like ES-DE, so `.jpeg` shows as missing (decided).
  A missing file reports the path it would get (`covers/<stem>.png`, `videos/<stem>.mp4`).
- **Image type:** the constant `Cst_EsdeImageType = "covers"` (decided); no settings key.
- **Writing:** `setImage`/`setVideo` write `covers/<stem>.png` and `videos/<stem>.mp4`, set no DOM
  tag and do not save the gamelist. A file of the same media in another format (`X.jpg` beside the
  new `X.png`) is removed after the commit, because it would hide the new one.
  `removeImage`/`removeVideo` delete the file unless another ROM with the stem uses it.
- **Removing a game:** deletes the ROM and that ROM's files in all 12 ES-DE type folders (decided),
  unless another game has the same stem. Files go to the trash.
- **Linking and renaming** return an Error in this layout; the two menu actions show a notice
  instead. `unlinkedMedia`/`misnamedMedia` are not reached.
- **Known limit:** after a write, another game with the same stem keeps its old paths until reload.
- **Not checked against the sample card:** it was not mounted when Phase 2 landed; the tests use a
  fixture in the same shape.

### Phase 3: UI — done

- **Opening a root:** the dialog already accepted both kinds (Phase 1). The window title reads
  `GameList Editor Qt - ES-DE` while an ES-DE system is loaded (set in `bindGamelist`).
- **Menus:** link/rename show a notice in the ES-DE view (Phase 2).
- **`GameEditPanel`:** shows `physicalImagePath` as before; the delete prompt says "all its media"
  in the ES-DE view.
- **Help text:** names the ES-DE folder, the title indicator, and that ES-DE must be stopped.
- **Scraping (optional):** `ScrapePanel::finishSave` could save every downloaded type into its
  own folder. YAGNI until someone asks for it. Mapping:

| Scrape type | ES-DE folder |
|---|---|
| `box-2D` | `covers` |
| `ss` | `screenshots` |
| `sstitle` | `titlescreens` |
| `box-3D` | `3dboxes` |
| `wheel` | `marquees` |
| `mixrbv*` | `miximages` |
| `video` | `videos` |

## 3. Groundwork for the later sync (nothing built now)

Phase 0 already gives the sync what it needs: every game has a layout-independent `romPath`,
`imagePath` and `videoPath`. The sync itself would be:

- **First real case (the sample card):** legacy `ROMs/<sys>/gamelist.xml` and ES-DE
  `gamelists/<sys>/gamelist.xml` share one ROM folder, so games match by `<path>` directly, and
  media maps `ROMs/<sys>/images/X.png` → `downloaded_media/<sys>/covers/X.png`,
  `videos/X.mp4` → `videos/X.mp4`.

- **System pairing:** a small mapping table of ES-DE ↔ Batocera names (`sg1000` ↔ `sg-1000`, …),
  keyed on the existing `kind`.
- **Game matching:** by `<path>` relative to each `romDir`, then by file name, then by CRC
  (already in `Hashes`).
- **Core function (widget-free, testable):**
  `diffGamelists( const Gamelist& a, const Gamelist& b ) -> QVector<SyncRow>`. Each row holds the
  index in A and B, the differing fields, and whether the media is missing on one side.
- **Dialog:** in the style of `F_LinkMedia`, with a direction per row (A→B, B→A, skip). Fields are
  applied with `setFields` + `changedFields`; media with `setImage`/`setVideo`, which handle both
  layouts after Phase 2.
- **Limits to state in the UI:**
  - With no stored base version there is no three-way merge; the user decides each conflict.
  - Batocera's extra media tags (`thumbnail`, `marquee`, `fanart`, `manual`) would need `Game` to
    carry more than one image. That is a sync v2.

## 4. Risks and open questions

1. ~~Multi-root `gamelist.xml`~~ **Confirmed** on the sample install; handled by Phase 1b.
2. ~~Which ES-DE type is "the" image?~~ **Decided:** `covers`, a constant.
3. ~~What should `removeGame` delete?~~ **Decided:** all 12 type folders.
4. **ES-DE systems with ROMs but no `gamelist.xml` yet:** list them in v1, or skip them like today.
5. **Portable `%ESPATH%` / Android:** "app-data parent, else ask" fits the sample card; still
   unverified on a desktop install with a custom `ROMDirectory`.
6. ~~`.jpeg` media~~ **Decided:** shown as missing, like ES-DE.

## 5. Tests to leave behind

- **`tst_Systems`:** an ES-DE fixture (`gamelists/snes/gamelist.xml`, `settings/es_settings.xml`
  with `%ESPATH%`, a ROMs folder) scans to the right `romDir`/`mediaDir`.
- **`tst_Gamelist`:**
  - Conventional resolution: `covers/Sub/Game.jpg` found for `./Sub/Game.zip`.
  - `setImage` writes `covers/…png`, adds no `<image>` tag and removes the `.jpg` sibling.
  - `removeGame` deletes the conventional files.
  - Multi-root round-trip: `<alternativeEmulator>` + `<gameList>` loads, saves, and keeps both
    roots in order (fixture copied from the sample `mame` file, descriptions trimmed).
- **Phase 0:** no new test; the existing suite is the check.
