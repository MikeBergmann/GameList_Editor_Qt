# Analysis

I started with a clone of https://github.com/andresdelcampo/GameList_Editor from Mar 22, 2025 (commit 8ea4f48).

Some stuff to make my life easier:
- I'll keep some of the names as they are in the original code. E.g., widget names. Makes the logic port a mechanical 1:1 map: `Edt_Name.Text` → `ui->Edt_Name->text()`.
- Split logic, GUI, and scraper.
- 

## Platform swaps

| Delphi | Qt6 |
|---|---|
| `U_gnugettext.pas` (4,317 lines) | deleted, no replacement — see English-only decision |
| `Xml.XMLDoc` + `msxmldom` | `QDomDocument` |
| `TNetHTTPClient`, `IdURI` | `QNetworkAccessManager` |
| `TIniFile` | `QSettings` |
| `TThread` + `Synchronize` (`U_DownloadThread.pas`) | signals/slots |

