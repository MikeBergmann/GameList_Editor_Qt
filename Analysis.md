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

The original code loaded and saved `gamelist.xml` several times through one shared TXMLDocument. Additionally, Widgets held the state that should have been in a model, e.g., by holding raw TGame* pointers. This was redesigned to Gamelist as the data layer for one emulator system’s gamelist.xml.

## Later

- Gradually replace the parallel arrays with a single structured table.
- Currently, my comments are very specific to the port from Delphi, sometimes including line numbers. Change them to something more general.
- I have a lot of constants, which could be enums. E.g. `case 11: return aGame.kidGame == 1;` reads better as `case FilterCategory::KidGame:`
- 
