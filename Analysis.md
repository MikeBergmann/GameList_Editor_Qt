# Analysis

I started with a clone of https://github.com/andresdelcampo/GameList_Editor from Mar 22, 2025 (commit 8ea4f48).

Some stuff to make my life easier:
- I'll keep some of the names as they are in the original code. E.g., widget names. Makes the logic port a mechanical 1:1 map: `Edt_Name.Text` → `ui->Edt_Name->text()`.
- Split logic, GUI, and scraper.

## Platform swaps

| Delphi | Qt6 |
|---|---|
| `U_gnugettext.pas` (4,317 lines) | deleted, no replacement — see English-only decision |
| `Xml.XMLDoc` + `msxmldom` | `QDomDocument` |
| `TNetHTTPClient`, `IdURI` | `QNetworkAccessManager` |
| `TIniFile` | `QSettings` |
| `TThread` + `Synchronize` (`U_DownloadThread.pas`) | signals/slots |

## Gamelist

The original code loaded and saved `gamelist.xml` several times through one shared TXMLDocument. Additionally, Widgets held the state (e.g., by holding raw TGame* pointers) that should have been in a model. This was redesigned to Gamelist as the data layer for one emulator system’s `gamelist.xml`.

The QDomDocument decision: I'm designing it to hold my gamelists, which are typically a few dozen of homebrew and legal available ROM-based games per system. Not tens of thousands and definitely not a full MAME set. So QDomDocument is fine here. Additionally, Qt dropped SAX with Qt6, and the nearest available replacement in Qt6 is QXmlStreamReader/QXmlStreamWriter, which I'm not getting warm with. Given we want to access a random game by index and edit/save that one node, QDomDocument should be the correct choice.

I resolve media links to the actual path on disk instead of relying on the media link. This is done because:

* Linux is case-sensitive, and game lists often come from Windows, which has a case-insensitive filesystem.
* In the future, I want to support ES-DE game lists, which don’t rely on <image> links. It finds media by convention, as downloaded_media/<system>/<type>/<romname>.<ext>. This allows me to implement a better resolver
* I plan to implement an On-Click repair of invalid media links, if possible, to rewrite the link from the resolved path.

## Later

- Gradually replace the parallel arrays with a single structured table.
- Currently, my comments are very specific to the port from Delphi, sometimes including line numbers. Change them to something more general.
- I have a lot of constants, which could be enums. E.g. `case 11: return aGame.kidGame == 1;` reads better as `case FilterCategory::KidGame:`
- 
