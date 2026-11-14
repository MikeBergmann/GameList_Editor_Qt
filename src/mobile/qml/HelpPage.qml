import QtQuick

TextPage {
   title: "Help"
   html: `
<h3>Getting started</h3>
<ul>
<li>The app needs the <b>All files access</b> permission to read and write your ROM folders. It
asks on first start.</li>
<li>Press <b>Choose folder</b> and pick the folder that holds your system folders, or the ES-DE
folder (the one with <code>gamelists</code> and <code>settings</code>). It is opened again on the
next start.</li>
<li><b>Stop EmulationStation (or ES-DE) first.</b> It rewrites <code>gamelist.xml</code> when it
exits and would overwrite anything saved here.</li>
</ul>

<h3>Systems</h3>
<p>Tap a system to see its games. A folder the app does not know has no logo: hold it (Start on
a gamepad) to tell the app which system it is.</p>

<h3>Games</h3>
<ul>
<li><b>Search</b> filters by name, and the drop-down below filters by category: missing
picture, video or other fields, favorites, duplicates, or games like the one last opened.</li>
<li><b>List by ROM</b> shows the ROM file names instead of the game names. <b>Full ROM name</b>
keeps the extension.</li>
<li><b>Actions, Add missing ROMs</b> lists the ROMs of the folder that the gamelist lacks. All
are ticked; <b>Add</b> writes the ticked ones.</li>
</ul>

<h3>Editing a game</h3>
<ul>
<li>Tap a field to change it. <b>Save</b> turns on once something changed. Leaving the page
asks whether to save or discard.</li>
<li>Dates are written <code>dd/mm/yyyy</code>. Kid game, Hidden and Favorite switch on a tap.</li>
<li>Swipe the picture left or right (L1 / R1 or Left / Right on a gamepad) for the next or
previous game of the list. Unsaved changes ask first.</li>
<li><b>Play</b> plays the game's video. <b>More info</b> shows its files and play data, and
<b>Calculate hashes</b> computes CRC32, MD5 and SHA1 of the ROM.</li>
</ul>

<h3>Pictures and videos</h3>
<ul>
<li><b>Change picture</b> and <b>Change video</b> open a file chooser. Pictures can be png, jpg
or webp and are saved as PNG; a video must be an mp4 and is copied as it is. Both are named
after the ROM.</li>
<li><b>Remove</b> deletes the file, unless another game uses it.</li>
<li>With an ES-DE folder, media is found by the ROM's name in ES-DE's media folder and the
gamelist is not changed. ES-DE reads only jpg, png and webp pictures.</li>
</ul>

<h3>Scraping</h3>
<ul>
<li>Enter your screenscraper.fr login and password in <b>Settings</b>. The same page sets the
scrape language and a proxy.</li>
<li>Open a game and press <b>Scrape</b>, then <b>Scrape now</b>. The game is looked up by the
CRC32 of its ROM, or by the CRC32 you type in, so a large ROM takes a moment to hash.</li>
<li>Check the results, edit any text, tap a picture to keep it, and use <b>Fetch</b> to choose
the picture types to download. <b>Save infos</b>, <b>Save picture</b> and <b>Save video</b>
write what is set to <i>Yes</i>.</li>
<li>If the service does not know the ROM you can pick the closest game or search by name. Each
search counts against your ScreenScraper quota.</li>
<li>Scrape is off while the game has unsaved changes or its ROM is missing.</li>
</ul>

<h3>Gamepad</h3>
<ul>
<li>D-pad moves, <b>A</b> selects, <b>B</b> goes back, <b>Start</b> sets a system's type.</li>
<li><b>L1 / R1</b> move to the previous or next game in the editor, and scroll this page.</li>
</ul>

<h3>Not on Android yet</h3>
<p>Editing several games at once, deleting games, linking and renaming media, syncing to another
folder and the LaunchBox scraper are in the desktop version.</p>
`
}
