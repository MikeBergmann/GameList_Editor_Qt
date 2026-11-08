import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
   id: page
   StackView.onActivated: list.forceActiveFocus()

   // Limitation: a typed path until the file browser exists (Android.md, Phase 2).
   function openRoot() {
      if ( library.openRoot( pathField.text ) )
         list.forceActiveFocus()
   }

   Component.onCompleted: {
      if ( library.lastFolder )
         library.openRoot( library.lastFolder )
   }

   Component { id: gamesPage; GamesPage {} }

   header: ToolBar {
      Label {
         anchors.centerIn: parent
         text: "GameList Editor"
         font.pixelSize: 20
      }
   }

   ColumnLayout {
      anchors.fill: parent
      anchors.margins: 8
      spacing: 8

      RowLayout {
         Layout.fillWidth: true

         // Click focus only: the D-pad must not land here, the soft keyboard would cover the list.
         TextField {
            id: pathField
            Layout.fillWidth: true
            focusPolicy: Qt.ClickFocus
            text: library.rootPath || library.lastFolder || "/storage/emulated/0"
            inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            onAccepted: page.openRoot()
         }
         Button {
            id: editBtn
            text: "Edit"
            onClicked: pathField.forceActiveFocus()
            KeyNavigation.right: openBtn
            KeyNavigation.down: list
         }
         Button {
            id: openBtn
            text: "Open"
            onClicked: page.openRoot()
            KeyNavigation.left: editBtn
            KeyNavigation.down: list
         }
      }

      Label {
         Layout.fillWidth: true
         visible: library.error.length > 0
         text: library.error
         color: "#c62828"
         wrapMode: Text.Wrap
      }

      Label {
         Layout.fillWidth: true
         visible: library.systems.length === 0 && library.error.length === 0
         text: "Enter the folder that holds your ROM systems, then press Open."
         wrapMode: Text.Wrap
      }

      ListView {
         id: list
         Layout.fillWidth: true
         Layout.fillHeight: true
         clip: true
         model: library.systems
         KeyNavigation.up: openBtn

         delegate: ItemDelegate {
            width: ListView.view.width
            height: 64
            highlighted: ListView.isCurrentItem

            contentItem: RowLayout {
               spacing: 12
               Image {
                  source: modelData.logo
                  sourceSize.height: 96
                  fillMode: Image.PreserveAspectFit
                  Layout.preferredWidth: 96
                  Layout.preferredHeight: 48
               }
               Label {
                  Layout.fillWidth: true
                  text: modelData.name
                  elide: Text.ElideRight
               }
            }

            onClicked: {
               list.currentIndex = index
               if ( library.selectSystem( index ) )
                  page.StackView.view.push( gamesPage )
            }
         }
      }
   }
}
