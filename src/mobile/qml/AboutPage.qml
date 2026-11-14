import QtQuick

TextPage {
   title: "About"
   html: ( Qt.application.version.length > 0 ? "<p><b>Version " + Qt.application.version + "</b></p>" : "" ) + `
<p>GameList Editor Qt edits the <code>gamelist.xml</code> files of an
<a href="https://emulationstation.org/">EmulationStation</a> or
<a href="https://es-de.org/">ES-DE</a> installation, and scrapes missing game data and media
from <a href="https://www.screenscraper.fr">screenscraper.fr</a>.</p>

<p>Written by NeeeeB and andresdelcampo, originally in Delphi.<br>
Original source code:
<a href="https://github.com/andresdelcampo/GameList_Editor">github.com/andresdelcampo/GameList_Editor</a></p>

<p>This is an AI assisted port to C++/Qt6, with a Qt Quick front end on Android. Source code:
<a href="https://github.com/MikeBergmann/GameList_Editor_Qt">github.com/MikeBergmann/GameList_Editor_Qt</a></p>

<p>Console and brand logos are trademarks of their respective owners and are used for
identification only. Game metadata comes from screenscraper.fr, whose terms apply.</p>
`
}
