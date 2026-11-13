import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
   id: page
   readonly property bool wide: width > height
   StackView.onActivated: list.forceActiveFocus()
   Component.onCompleted: list.forceActiveFocus()

   readonly property var rows: [
      { key: "name", label: "Name", type: "text" },
      { key: "releaseDate", label: "Date (dd/mm/yyyy)", type: "text" },
      { key: "genre", label: "Genre", type: "text" },
      { key: "region", label: "Region", type: "text" },
      { key: "players", label: "Players", type: "text" },
      { key: "rating", label: "Rating", type: "text" },
      { key: "developer", label: "Developer", type: "text" },
      { key: "publisher", label: "Publisher", type: "text" },
      { key: "description", label: "Description", type: "multiline" },
      { key: "kidGame", label: "Kid game", type: "flag" },
      { key: "hidden", label: "Hidden", type: "flag" },
      { key: "favorite", label: "Favorite", type: "flag" },
      { key: "", label: "Change picture", type: "changeImage" },
      { key: "", label: "Change video", type: "changeVideo" },

      { key: "", label: "Remove picture", type: "removeImage" },
      { key: "", label: "Remove video", type: "removeVideo" },
      { key: "", label: "More info", type: "moreInfo" }
   ]

   function requestClose() {
      leaveDialog.pendingStep = 0
      if ( edit.dirty )
         leaveDialog.open()
      else
         page.StackView.view.pop()
   }

   // Dirty edits ask first; otherwise the window's handler pops the page.
   function guardClose( aEvent ) {
      if ( edit.dirty ) {
         leaveDialog.pendingStep = 0
         leaveDialog.open()
      } else {
         aEvent.accepted = false
      }
   }
   Keys.onEscapePressed: ( aEvent ) => guardClose( aEvent )
   Keys.onBackPressed: ( aEvent ) => guardClose( aEvent )

   // Previous / next game of the filtered list. Unsaved edits ask first, as when leaving.
   function goTo( aDelta ) {
      if ( edit.dirty ) {
         leaveDialog.pendingStep = aDelta
         leaveDialog.open()
      } else {
         games.step( aDelta )
      }
   }
   // L1 / R1; the D-pad does the same in the field list, where Left / Right are free.
   Keys.onPressed: ( aEvent ) => {
      if ( aEvent.key === Qt.Key_PageUp || aEvent.key === Qt.Key_PageDown ) {
         aEvent.accepted = true
         page.goTo( aEvent.key === Qt.Key_PageUp ? -1 : 1 )
      }
   }

   function activateRow( aRow ) {
      if ( aRow.type === "flag" ) {
         edit.setField( aRow.key, edit.values[ aRow.key ] ? 0 : 1 )
      } else if ( aRow.type === "moreInfo" ) {
         infoDialog.open()

      } else if ( aRow.type === "changeImage" || aRow.type === "changeVideo" ) {
         const isImage = aRow.type === "changeImage"
         const picker = page.StackView.view.push( browserPage,
            { suffixes: isImage ? [ "png", "jpg", "jpeg", "webp" ] : [ "mp4" ] } )
         picker.picked.connect( ( aPath ) => isImage ? edit.changeImage( aPath ) : edit.changeVideo( aPath ) )
      } else if ( aRow.type === "removeImage" || aRow.type === "removeVideo" ) {
         deleteDialog.what = aRow.type
         deleteDialog.open()
      } else {
         fieldDialog.title = aRow.label
         fieldDialog.key = aRow.key
         fieldDialog.multiline = aRow.type === "multiline"
         fieldDialog.value = edit.values[ aRow.key ] || ""
         fieldDialog.open()
      }
   }

   FieldDialog {
      id: fieldDialog
      property string key
      onCommitted: ( aText ) => edit.setField( key, aText )
      onClosed: list.forceActiveFocus()
   }

   Component { id: scrapePage; ScrapePage {} }
   Component { id: browserPage; FileBrowserPage {} }

   InfoDialog {
      id: infoDialog
      onClosed: list.forceActiveFocus()
   }

   VideoDialog {
      id: videoDialog
      onClosed: list.forceActiveFocus()
   }

   AskDialog {
      id: deleteDialog
      property string what
      message: "Delete the " + ( what === "removeImage" ? "picture" : "video" )
               + " of this game? The file is deleted unless another game uses it."
      choices: [ "Delete", "Cancel" ]
      onChosen: ( aChoice ) => {
         if ( aChoice === 0 )
            what === "removeImage" ? edit.removeImage() : edit.removeVideo()
         list.forceActiveFocus()
      }
   }

   AskDialog {
      id: leaveDialog
      // 0: leave the page; otherwise the step to take afterwards.
      property int pendingStep: 0
      message: "Save the changes to this game?"
      choices: [ "Save", "Discard", "Cancel" ]
      function proceed() {
         if ( pendingStep === 0 )
            page.StackView.view.pop()
         else
            games.step( pendingStep )
      }
      onChosen: ( aChoice ) => {
         if ( aChoice === 0 ) {
            if ( edit.save() )
               proceed()
         } else if ( aChoice === 1 ) {
            edit.revert()
            proceed()
         }
         list.forceActiveFocus()
      }
   }

   header: ToolBar {
      RowLayout {
         anchors.fill: parent

         BarButton {
            id: backBtn
            text: "Back"
            onClicked: page.requestClose()
            KeyNavigation.right: playBtn
            KeyNavigation.down: list
         }
         Label {
            Layout.fillWidth: true
            text: edit.values.name || ""
            font.pixelSize: 20
            elide: Text.ElideRight
         }
         BarButton {
            id: playBtn
            text: "Play"
            enabled: edit.hasVideo
            onClicked: videoDialog.open()
            KeyNavigation.left: backBtn
            KeyNavigation.right: scrapeBtn
            KeyNavigation.down: list
         }
         // Limitation: a scrape save reloads the fields, so typed edits are saved or reverted first.
         BarButton {
            id: scrapeBtn
            text: "Scrape"
            enabled: !edit.dirty && scrape.canScrape
            onClicked: page.StackView.view.push( scrapePage )
            KeyNavigation.left: playBtn
            KeyNavigation.right: saveBtn
            KeyNavigation.down: list
         }
         BarButton {
            id: saveBtn
            text: "Save"
            enabled: edit.dirty
            onClicked: edit.save()
            KeyNavigation.left: scrapeBtn
            KeyNavigation.down: list
         }
      }
   }

   Component {
      id: picture

      Item {
         id: pic
         clip: true
         property real offset: 0
         property int slideDir: 0
         property bool switched: false

         NumberAnimation {
            id: settle
            target: pic
            property: "offset"
            to: 0
            duration: 200
            easing.type: Easing.OutCubic
         }

         // The new game slides in from the side the swipe came from.
         Connections {
            target: games
            function onCurrentChanged() {
               pic.switched = true
               if ( pic.slideDir !== 0 ) {
                  pic.offset = pic.slideDir * pic.width
                  settle.restart()
               }
            }
         }

         // Swipe left: next game, right: previous. Horizontal only; a vertical drag goes to the list.
         MouseArea {
            anchors.fill: parent
            property real pressX: 0
            property real pressY: 0
            onPressed: ( aMouse ) => {
               settle.stop()
               pressX = aMouse.x
               pressY = aMouse.y
            }
            // Once the drag is mostly horizontal, keep it: the list would otherwise steal it on any vertical drift.
            onPositionChanged: ( aMouse ) => {
               const dx = aMouse.x - pressX
               if ( Math.abs( dx ) > 4 && Math.abs( dx ) > Math.abs( aMouse.y - pressY ) )
                  preventStealing = true
               pic.offset = dx
            }
            onCanceled: {
               preventStealing = false
               settle.restart()
            }
            onReleased: ( aMouse ) => {
               preventStealing = false
               const dx = aMouse.x - pressX
               pic.switched = false
               if ( Math.abs( dx ) > 80 ) {
                  pic.slideDir = dx < 0 ? 1 : -1
                  page.goTo( pic.slideDir )
                  pic.slideDir = 0
               }
               // Too short, at either end, or waiting on the save dialog: snap back.
               if ( !pic.switched )
                  settle.restart()
            }
         }

         Item {
            x: pic.offset
            width: pic.width
            height: pic.height

            Image {
               anchors.fill: parent
               anchors.margins: 8
               source: edit.imageSource
               fillMode: Image.PreserveAspectFit
               sourceSize.height: 800
               asynchronous: true
               cache: false
            }
            Label {
               anchors.centerIn: parent
               visible: edit.imageSource === ""
               text: "No picture"
            }
         }
      }
   }

   Component {
      id: pictureHeader

      Item {
         width: ListView.view.width
         height: 200

         Loader {
            anchors.fill: parent
            sourceComponent: picture
         }
      }
   }

   ColumnLayout {
      anchors.fill: parent
      spacing: 0

      Label {
         Layout.fillWidth: true
         Layout.margins: 8
         visible: edit.error.length > 0
         text: edit.error
         color: "#c62828"
         wrapMode: Text.Wrap
      }

      RowLayout {
         Layout.fillWidth: true
         Layout.fillHeight: true
         spacing: 0

         // Landscape: the picture is a fixed column; portrait: it scrolls as the list header.
         Loader {
            active: page.wide
            visible: active
            Layout.preferredWidth: page.width / 3
            Layout.fillHeight: true
            sourceComponent: picture
         }

         ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: page.rows
            KeyNavigation.up: backBtn
            header: page.wide ? null : pictureHeader
            Keys.onLeftPressed: page.goTo( -1 )
            Keys.onRightPressed: page.goTo( 1 )

            delegate: ItemDelegate {
               id: row
               width: ListView.view.width
               highlighted: ListView.isCurrentItem
               enabled: modelData.type === "removeImage" ? edit.imageSource !== ""
                        : modelData.type === "removeVideo" ? edit.hasVideo : true

               readonly property bool isValue: modelData.type === "text" || modelData.type === "multiline"
                                               || modelData.type === "flag"
               readonly property var raw: edit.values[ modelData.key ]
               readonly property string shown: modelData.type === "flag" ? ( raw ? "Yes" : "No" ) : ( raw || "" )

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
