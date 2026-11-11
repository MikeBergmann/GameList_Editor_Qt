import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Scrapes the selected game from ScreenScraper. Rows work like the edit page's: A (or a tap)
// changes one, the thumbnails pick the picture to keep, and Save writes what is ticked.
Page {
   id: page
   readonly property bool wide: width > height
   property bool justSaved: false

   StackView.onActivated: list.forceActiveFocus( Qt.TabFocusReason )
   Component.onDestruction: scrape.reset()

   // Scraper cannot be cancelled, so leaving mid-request would fill the wrong game.
   function swallowWhileBusy( aEvent ) {
      aEvent.accepted = scrape.busy
   }
   Keys.onEscapePressed: ( aEvent ) => swallowWhileBusy( aEvent )
   Keys.onBackPressed: ( aEvent ) => swallowWhileBusy( aEvent )

   readonly property var mediaChoices: [
      [ "box2d", "Box 2D" ], [ "box3d", "Box 3D" ], [ "mix1", "Mix 1" ],
      [ "mix2", "Mix 2" ], [ "screenshot", "Screenshot" ], [ "title", "Title" ],
      [ "arcadeBox", "Arcade box" ], [ "wheel", "Wheel" ], [ "video", "Video" ]
   ]

   readonly property var rows: {
      const base = [
         { type: "scrape", label: "Scrape now" },
         { type: "media", label: "Fetch" },
         { type: "crc", label: "CRC32 (empty: hash the ROM)" }
      ]
      if ( !scrape.hasResult )
         return base

      const texts = [
         [ "name", "Name" ], [ "releaseDate", "Date (dd/mm/yyyy)" ], [ "genre", "Genre" ],
         [ "region", "Region" ], [ "players", "Players" ], [ "rating", "Rating" ],
         [ "developer", "Developer" ], [ "publisher", "Publisher" ], [ "description", "Description" ]
      ].map( ( aPair ) => ( { type: "text", key: aPair[ 0 ], label: aPair[ 1 ] } ) )

      const saves = [ { type: "save", key: "saveInfos", label: "Save infos" },
                      { type: "save", key: "savePicture", label: "Save picture" } ]
      if ( scrape.hasVideo )
         saves.push( { type: "save", key: "saveVideo", label: "Save video" } )
      return base.concat( texts, saves )
   }

   function activateRow( aRow ) {
      if ( aRow.type === "scrape" ) {
         scrape.scrape()
      } else if ( aRow.type === "media" ) {
         mediaDialog.open()
      } else if ( aRow.type === "save" ) {
         scrape[ aRow.key ] = !scrape[ aRow.key ]
      } else {
         fieldDialog.title = aRow.label
         fieldDialog.key = aRow.type === "crc" ? "crc" : aRow.key
         fieldDialog.multiline = aRow.key === "description"
         fieldDialog.value = aRow.type === "crc" ? scrape.manualCrc : ( scrape.values[ aRow.key ] || "" )
         fieldDialog.open()
      }
   }

   Connections {
      target: scrape
      function onSaved() {
         page.justSaved = true
         edit.revert()
      }
      function onBusyChanged() {
         if ( scrape.busy )
            page.justSaved = false
      }
      function onPickingChanged() {
         if ( scrape.picking && !pickDialog.opened )
            pickDialog.open()
         else if ( !scrape.picking && pickDialog.opened )
            pickDialog.close()
      }
   }

   FieldDialog {
      id: fieldDialog
      property string key
      onCommitted: ( aText ) => {
         if ( key === "crc" )
            scrape.manualCrc = aText
         else
            scrape.setField( key, aText )
      }
      onClosed: list.forceActiveFocus( Qt.TabFocusReason )
   }

   // A popup's keys do not reach the window's handler, so A and the D-pad are handled here.
   function moveFocus( aItem, aForward ) {
      const next = aItem.nextItemInFocusChain( aForward )
      if ( next )
         next.forceActiveFocus( Qt.TabFocusReason )
   }

   Dialog {
      id: mediaDialog
      title: "Fetch"
      modal: true
      anchors.centerIn: Overlay.overlay
      width: Math.min( 480, Overlay.overlay.width - 32 )
      closePolicy: Popup.CloseOnEscape
      onOpened: checks.itemAt( 0 ).forceActiveFocus( Qt.TabFocusReason )
      onClosed: list.forceActiveFocus( Qt.TabFocusReason )

      contentItem: ColumnLayout {
         GridLayout {
            columns: page.wide ? 3 : 2
            Repeater {
               id: checks
               model: page.mediaChoices

               CheckBox {
                  id: box
                  text: modelData[ 1 ]
                  checked: scrape.media[ modelData[ 0 ] ]
                  onToggled: scrape.setMedia( modelData[ 0 ], checked )
                  Keys.onReturnPressed: click()
                  Keys.onEnterPressed: click()
                  Keys.onDownPressed: page.moveFocus( box, true )
                  Keys.onUpPressed: page.moveFocus( box, false )
               }
            }
         }
         Button {
            id: doneBtn
            Layout.alignment: Qt.AlignRight
            text: "Done"
            onClicked: mediaDialog.close()
            Keys.onReturnPressed: click()
            Keys.onEnterPressed: click()
            Keys.onUpPressed: page.moveFocus( doneBtn, false )
         }
      }
   }

   FieldDialog {
      id: searchDialog
      title: "Search ScreenScraper"
      onCommitted: ( aText ) => scrape.searchCandidates( aText )
      onClosed: pickList.forceActiveFocus( Qt.TabFocusReason )
   }

   Dialog {
      id: pickDialog
      modal: true
      parent: Overlay.overlay
      x: 12
      y: 8
      width: Overlay.overlay.width - 24
      height: Overlay.overlay.height - 16
      closePolicy: Popup.CloseOnEscape
      onOpened: ( scrape.candidates.length > 0 ? pickList : searchBtn ).forceActiveFocus( Qt.TabFocusReason )
      onClosed: {
         if ( scrape.picking )
            scrape.cancelPick()
         list.forceActiveFocus( Qt.TabFocusReason )
      }

      contentItem: ColumnLayout {
         spacing: 8

         Label {
            Layout.fillWidth: true
            text: "ScreenScraper has no game for this ROM. Pick the closest one, or change the name "
                  + "and search again (each search counts against your ScreenScraper quota)."
            wrapMode: Text.Wrap
            font.pixelSize: 13
         }
         Label {
            Layout.fillWidth: true
            visible: scrape.error.length > 0
            text: scrape.error
            color: "#c62828"
            wrapMode: Text.Wrap
         }
         Button {
            id: searchBtn
            Layout.fillWidth: true
            text: "Search: " + scrape.searchText
            onClicked: {
               searchDialog.value = scrape.searchText
               searchDialog.open()
            }
            Keys.onReturnPressed: click()
            Keys.onEnterPressed: click()
            KeyNavigation.down: pickList
         }
         ListView {
            id: pickList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: scrape.candidates
            KeyNavigation.up: searchBtn
            KeyNavigation.down: cancelBtn

            delegate: ItemDelegate {
               width: ListView.view.width
               text: modelData
               highlighted: ListView.isCurrentItem
               onClicked: scrape.pickCandidate( index )
               Keys.onReturnPressed: click()
               Keys.onEnterPressed: click()
            }
         }
         Button {
            id: cancelBtn
            Layout.alignment: Qt.AlignRight
            text: "Cancel"
            onClicked: pickDialog.close()
            Keys.onReturnPressed: click()
            Keys.onEnterPressed: click()
            KeyNavigation.up: pickList
         }
      }
   }

   header: ToolBar {
      RowLayout {
         anchors.fill: parent

         ToolButton {
            id: backBtn
            text: "Back"
            enabled: !scrape.busy
            onClicked: page.StackView.view.pop()
            KeyNavigation.right: saveBtn
            KeyNavigation.down: strip.visible ? strip : list
         }
         Label {
            Layout.fillWidth: true
            text: "Scrape: " + ( edit.values.name || "" )
            font.pixelSize: 20
            elide: Text.ElideRight
         }
         ToolButton {
            id: saveBtn
            text: "Save"
            enabled: scrape.canSave
            onClicked: scrape.save()
            KeyNavigation.left: backBtn
            KeyNavigation.down: strip.visible ? strip : list
         }
      }
   }

   GridLayout {
      anchors.fill: parent
      columns: page.wide ? 2 : 1
      rowSpacing: 0
      columnSpacing: 0

      ColumnLayout {
         Layout.fillWidth: !page.wide
         Layout.fillHeight: page.wide
         Layout.preferredWidth: page.wide ? page.width / 3 : -1
         Layout.preferredHeight: page.wide ? -1 : 240
         spacing: 0

         Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Image {
               anchors.fill: parent
               anchors.margins: 8
               visible: scrape.selectedPicture >= 0
               source: scrape.selectedPicture >= 0
                       ? "image://scrape/" + scrape.generation + "/" + scrape.selectedPicture : ""
               fillMode: Image.PreserveAspectFit
               sourceSize.height: 800
               cache: false
            }
            Label {
               anchors.centerIn: parent
               width: parent.width - 16
               horizontalAlignment: Text.AlignHCenter
               wrapMode: Text.Wrap
               visible: scrape.selectedPicture < 0
               text: scrape.pictureCount > 0 ? "Pick a picture below"
                     : scrape.hasResult ? "No pictures" : ""
            }
         }

         ListView {
            id: strip
            Layout.fillWidth: true
            Layout.preferredHeight: 96
            visible: scrape.pictureCount > 0
            orientation: ListView.Horizontal
            clip: true
            spacing: 4
            model: scrape.pictureCount
            KeyNavigation.up: backBtn
            KeyNavigation.down: list

            delegate: ItemDelegate {
               width: 96
               height: 96
               highlighted: index === scrape.selectedPicture || ListView.isCurrentItem
               contentItem: Image {
                  source: "image://scrape/" + scrape.generation + "/" + index
                  fillMode: Image.PreserveAspectFit
                  sourceSize.height: 160
                  cache: false
               }
               onClicked: {
                  strip.currentIndex = index
                  scrape.selectPicture( index )
               }
            }
         }
      }

      ColumnLayout {
         Layout.fillWidth: true
         Layout.fillHeight: true
         spacing: 0

         ProgressBar {
            Layout.fillWidth: true
            visible: scrape.busy
            indeterminate: true
         }
         Label {
            Layout.fillWidth: true
            Layout.margins: 8
            visible: scrape.error.length > 0 && !pickDialog.opened
            text: scrape.error
            color: "#c62828"
            wrapMode: Text.Wrap
         }
         Label {
            Layout.fillWidth: true
            Layout.margins: 8
            visible: page.justSaved
            text: "Saved."
         }

         ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            enabled: !scrape.busy
            model: page.rows
            KeyNavigation.up: strip.visible ? strip : backBtn

            delegate: ItemDelegate {
               id: row
               width: ListView.view.width
               highlighted: ListView.isCurrentItem
               enabled: modelData.type === "scrape" ? scrape.canScrape
                        : modelData.key === "savePicture" ? scrape.selectedPicture >= 0 : true

               readonly property bool isValue: modelData.type !== "scrape"
               readonly property string shown:
                  modelData.type === "media" ? ( Object.keys( scrape.media ).filter( ( aKey ) => scrape.media[ aKey ] ).length + " selected" )
                  : modelData.type === "crc" ? scrape.manualCrc
                  : modelData.type === "save" ? ( scrape[ modelData.key ] ? "Yes" : "No" )
                  : ( scrape.values[ modelData.key ] || "" )

               contentItem: ColumnLayout {
                  spacing: 0

                  Label {
                     Layout.fillWidth: true
                     text: modelData.label
                     font.pixelSize: row.isValue ? 12 : 16
                     opacity: row.isValue ? 0.7 : 1
                  }
                  Label {
                     Layout.fillWidth: true
                     visible: row.isValue
                     text: row.shown.length > 0 ? row.shown : "-"
                     elide: Text.ElideRight
                     maximumLineCount: 2
                     wrapMode: Text.Wrap
                  }
               }

               onClicked: {
                  list.currentIndex = index
                  page.activateRow( modelData )
               }
            }
         }
      }
   }
}
