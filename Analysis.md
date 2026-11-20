# Analysis

I started with a clone of https://github.com/andresdelcampo/GameList_Editor from Mar 22, 2025 (commit 8ea4f48).

Some stuff to make my life easier:
- I'll keep some of the names as they are in the original code. E.g., widget names. Makes the logic port a mechanical 1:1 map: `Edt_Name.Text` → `ui->Edt_Name->text()`.
- Split logic, GUI, and scraper.

## Platform swaps

| Delphi | Qt6 |
|---|---|
| `U_gnugettext.pas` | deleted, no replacement — see English-only decision |
| `Xml.XMLDoc` + `msxmldom` | `QDomDocument` |
| `TNetHTTPClient`, `IdURI` | `QNetworkAccessManager` |
| `TIniFile` | `QSettings` |
| `TThread` + `Synchronize` (`U_DownloadThread.pas`) | signals/slots |

## Gamelist

The original code loaded and saved `gamelist.xml` several times through one shared TXMLDocument. Additionally, Widgets held the state (e.g., by holding raw TGame* pointers) that should have been in a model. This was redesigned to Gamelist as the data layer for one emulator system's `gamelist.xml`.

The QDomDocument decision: I'm designing it to hold my gamelists, which are typically a few dozen of homebrew and legal available ROM-based games per system. Not tens of thousands and definitely not a full MAME set. So QDomDocument is fine here. Additionally, Qt dropped SAX with Qt6, and the nearest available replacement in Qt6 is QXmlStreamReader/QXmlStreamWriter, which I'm not getting warm with. Given we want to access a random game by index and edit/save that one node, QDomDocument should be the correct choice. Additionally: each edit change that element in place. Other Elements survive a save untouched. A stream writer would have to know every element to write the file back.

Regarding media links: I resolve media links to the actual path on disk instead of relying on the link. This is done because:

- Linux is case-sensitive, and game lists often come from Windows, which has a case-insensitive filesystem.
- In the future, I want to support ES-DE game lists, which don't rely on `<image>` links. It finds media by convention, as `downloaded_media/<system>/<type>/<romname>.<ext>`. This allows me to implement a better resolver.
- I plan to implement an on-click repair of invalid media links, if possible, to rewrite the link from the resolved path.

Other Design decisions:

- `removeImage` / `removeVideo` delete the file only if no other game resolves to the same file. Several games can share one picture.
- New media goes into the folder of the first game that has any (`imageFolder()` / `videoFolder()`), named after the game. Images are always re-encoded to PNG. Videos are copied as they are.
- Every action that touches files saves by itself and leaves memory, DOM and files as they were if that save fails.

## Regarding build and test

The tests use their own `main()` and default to the offscreen  (so CI needs no X server). Tests should never touch the real config, so I redirect `QSettings` to a temporary directory. Platform-specific tests (e.g., case-sensitive filesystem)  skip themselves on the respective OS.

In CI, a Linux job (sanitize in `ci.yml`) runs the tests with Debug and ASan / UBSan. Because Qt and fontconfig report leaks that I cannot fix, leak detection is off. The sanitizers cannot run on MinGW (so Linux-only), and it only sees code the tests execute (e.g., the network side of the scraper is not covered).

I do not use Clazy. I think its bug checks, which target lambdas that outlive their object, are less helpful in Qt code in which every connect uses `this` as context and captures by value. Clazy's performance advice does not matter for a gamelist editor, I think. Additionally, it needs a Clang build with the plugin.

Regarding code coverage. My rule is: Non-trivial logic leaves a test behind. So, some areas are tested less intensively than others. E.g., the widget code is only smoke-tested, the scraper’s network path cannot run in CI, and platform-specific branches run only on the corresponding runner. A coverage percentage says nothing about this, and may only invite writing tests to reach high coverage scores. 

## Deviations from the Delphi original

The logic is a 1:1 port, except where I think the original was wrong or didn't fit Linux. Most of these are also marked in the code comments.

| Area | Delphi | Here | Why |
|---|---|---|---|
| Gamelist without any `<game>` | System dropped silently, document left active, hourglass stuck | `load()` fails with a message | Seems like a bug |
| Load / save | One shared `TXMLDocument`, reloaded several times | Parsed once, held open, one `<game>` handle per `Game` | Re-Design |
| Missing `<path>`, `<name>`, ... child | Unguarded `FindNode(...).Text` raised | Reads as empty | Seems like a bug |
| Media folder when no game has media | Empty, scraped art landed in the ROM folder | `./media/images/`, `./media/videos/` | EmulationStation's own layout |
| "Same folder" filter | `AnsiSameText`, case-insensitive | Case-sensitive compare after `resolvePath` | Two folders differing only in case are different on Linux |
| Path building | Hardcoded backslashes | `QDir`, forward slashes | Linux |
| Saving the file | Re-indented only after adding a node | Re-indents the whole document, via `QSaveFile` | Re-Design |
| Removing media | May delete the file because it compared only the links  | Deleted only if no other game uses it based on real path | Possible Data loss |
| Media in a sync | n/a (no sync) | Copied byte for byte; re-encoded to PNG only when the target (ES-DE) cannot read the format | "Always PNG" would turn every jpg and webp cover into a bigger file and make a second sync see every cover as changed |

## Later

- Secrets: The `QSettings` keys `SSPwd` and `ProxyPwd` (kept from the Delphi ini) store the ScreenScraper and proxy passwords in plain text.
