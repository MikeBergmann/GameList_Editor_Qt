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
   Component { id: settingsPage; SettingsPage {} }
   Component { id: helpPage; HelpPage {} }

   // Only a folder the app does not know can be mapped to a system type.
   function askKind( aIndex ) {
      if ( !library.canSetKind( aIndex ) ) {
         notice.message = "This folder name is already a known system."
         notice.open()
         return
      }
      kindDialog.system = aIndex
      kindDialog.folder = library.systems[ aIndex ].folder
      kindDialog.current = library.kindChoice( aIndex )
      kindDialog.open()
   }

   AskDialog {
      id: notice
      choices: [ "OK" ]
      onClosed: list.forceActiveFocus( Qt.TabFocusReason )
   }

   KindDialog {
      id: kindDialog
      property int system: -1
      onPicked: ( aChoice ) => {
         library.setKind( system, aChoice )
         // The model was replaced, which resets the highlighted row.
         list.currentIndex = system
      }
      onClosed: list.forceActiveFocus( Qt.TabFocusReason )
   }

   header: ToolBar {
      Label {
         anchors.centerIn: parent
         text: "GameList Editor"
         font.pixelSize: 20
      }
      BarButton {
         id: helpBtn
         anchors.left: parent.left
         anchors.verticalCenter: parent.verticalCenter
         text: "Help"
         onClicked: page.StackView.view.push( helpPage )
         KeyNavigation.right: settingsBtn
         KeyNavigation.down: chooseBtn
      }
      BarButton {
         id: settingsBtn
         anchors.right: parent.right
         anchors.verticalCenter: parent.verticalCenter
         text: "Settings"
         onClicked: page.StackView.view.push( settingsPage )
         KeyNavigation.left: helpBtn
         KeyNavigation.down: chooseBtn
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
            KeyNavigation.up: settingsBtn
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
         // Start on the pad is Menu; a touch screen holds the row instead.
         Keys.onMenuPressed: if ( currentIndex >= 0 ) page.askKind( currentIndex )

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

            onPressAndHold: {
               list.currentIndex = index
               page.askKind( index )
            }
            onClicked: {
               list.currentIndex = index
               if ( library.selectSystem( index ) )
                  page.StackView.view.push( gamesPage )
            }
         }
      }

      Label {
         Layout.fillWidth: true
         visible: library.systems.length > 0
         text: "Hold a system (Start on a gamepad) to set its type."
         font.pixelSize: 12
         opacity: 0.7
         wrapMode: Text.Wrap
      }
   }
}
