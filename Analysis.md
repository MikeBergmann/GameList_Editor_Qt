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

The QDomDocument decision: I'm designing it to hold my gamelists, which are typically a few dozen of homebrew and legal available ROM-based games per system. Not tens of thousands and definitely not a full MAME set. So QDomDocument is fine here. Additionally, Qt dropped SAX with Qt6, and the nearest available replacement in Qt6 is QXmlStreamReader/QXmlStreamWriter, which I'm not getting warm with. Given we want to access a random game by index and edit/save that one node, QDomDocument should be the correct choice. Additionally: each Game keeps a handle to its `<game>` element, and edits change that element in place. Other Elements survive a save untouched. A stream writer would have to know every element to write the file back.

Regarding media links: I resolve media links to the actual path on disk instead of relying on the link. This is done because:

- Linux is case-sensitive, and game lists often come from Windows, which has a case-insensitive filesystem.
- In the future, I want to support ES-DE game lists, which don't rely on `<image>` links. It finds media by convention, as `downloaded_media/<system>/<type>/<romname>.<ext>`. This allows me to implement a better resolver.
- I plan to implement an on-click repair of invalid media links, if possible, to rewrite the link from the resolved path.

Other Design decisions:

- `removeImage` / `removeVideo` delete the file only if no other game resolves to the same file. Several games can share one picture.
- New media goes into the folder of the first game that has any (`imageFolder()` / `videoFolder()`), named after the game. Images are always re-encoded to PNG. Videos are copied as they are.

## Build and tests

- C++17, Qt 6.4 minimum (what my Mint 22 ships), warnings `-Wall -Wextra -Wshadow -Wconversion`.
- zlib is linked directly because Qt has no CRC32. It is a build dependency (`zlib1g-dev`).
- Tests use their own `main()` because widgets need a `QApplication`. It defaults to the `offscreen` platform (so CI needs no X server) and redirects `QSettings` to a temporary directory so tests never touch the real config.
- CI is Linux only (Ubuntu 24.04, `ctest`).

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
| Removing media | Deleted the file | Deleted only if no other game uses it | Possible Data loss |

## Later

- Open question: Gradually replace the parallel arrays with a single structured table.
- Currently, my comments are very specific to the port from Delphi, sometimes including line numbers. Change them to something more general.
- I have a lot of constants, which could be enums. E.g. `case 11: return aGame.kidGame == 1;` reads better as `case FilterCategory::KidGame:`
- Secrets: The `QSettings` keys `SSPwd` and `ProxyPwd` (kept from the Delphi ini) would store the ScreenScraper and proxy passwords in plain text. Decide before the scraper is ported: plain INI as before, or the system keyring.
