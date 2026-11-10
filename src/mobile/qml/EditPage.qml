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
      { key: "", label: "Remove picture", type: "removeImage" },
      { key: "", label: "Remove video", type: "removeVideo" }
   ]

   function requestClose() {
      if ( edit.dirty )
         leaveDialog.open()
      else
         page.StackView.view.pop()
   }

   // Dirty edits ask first; otherwise the window's handler pops the page.
   function guardClose( aEvent ) {
      if ( edit.dirty )
         leaveDialog.open()
      else
         aEvent.accepted = false
   }
   Keys.onEscapePressed: ( aEvent ) => guardClose( aEvent )
   Keys.onBackPressed: ( aEvent ) => guardClose( aEvent )

   function activateRow( aRow ) {
      if ( aRow.type === "flag" ) {
         edit.setField( aRow.key, edit.values[ aRow.key ] ? 0 : 1 )
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
      message: "Save the changes to this game?"
      choices: [ "Save", "Discard", "Cancel" ]
      onChosen: ( aChoice ) => {
         if ( aChoice === 0 ) {
            if ( edit.save() )
               page.StackView.view.pop()
         } else if ( aChoice === 1 ) {
            edit.revert()
            page.StackView.view.pop()
         } else {
            list.forceActiveFocus()
         }
      }
   }

   header: ToolBar {
      RowLayout {
         anchors.fill: parent

         ToolButton {
            id: backBtn
            text: "Back"
            onClicked: page.requestClose()
            KeyNavigation.right: saveBtn
            KeyNavigation.down: list
         }
         Label {
            Layout.fillWidth: true
            text: edit.values.name || ""
            font.pixelSize: 20
            elide: Text.ElideRight
         }
         ToolButton {
            id: saveBtn
            text: "Save"
            enabled: edit.dirty
            onClicked: edit.save()
            KeyNavigation.left: backBtn
            KeyNavigation.down: list
         }
      }
   }

   Component {
      id: picture

      Item {
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
