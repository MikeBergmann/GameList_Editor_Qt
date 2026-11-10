import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
   id: page

   function focusStart() {
      if ( list.activeFocus || chooseBtn.activeFocus || page.StackView.status !== StackView.Active )
         return
      ( library.systems.length > 0 ? list : chooseBtn ).forceActiveFocus( Qt.TabFocusReason )
   }

   StackView.onActivated: focusStart()

   // At startup the page is built before the window is shown or active, and focus set then is lost.
   Window.onWindowChanged: Qt.callLater( focusStart )
   Connections {
      target: page.Window.window
      function onActiveChanged() { if ( page.Window.window.active ) focusStart() }
   }

   Component.onCompleted: {
      if ( library.lastFolder )
         library.openRoot( library.lastFolder )
      Qt.callLater( focusStart )
   }

   Component { id: browserPage; FileBrowserPage {} }
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

         Label {
            Layout.fillWidth: true
            text: library.rootPath || "No folder chosen"
            elide: Text.ElideMiddle
         }
         Button {
            id: chooseBtn
            text: "Choose folder"
            onClicked: page.StackView.view.push( browserPage )
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
         text: "Choose the folder that holds your ROM systems."
         wrapMode: Text.Wrap
      }

      ListView {
         id: list
         Layout.fillWidth: true
         Layout.fillHeight: true
         clip: true
         model: library.systems
         KeyNavigation.up: chooseBtn

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
