import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
   id: page
   StackView.onActivated: list.forceActiveFocus()

   header: ToolBar {
      RowLayout {
         anchors.fill: parent

         ToolButton {
            id: backBtn
            text: "Back"
            onClicked: page.StackView.view.pop()
            KeyNavigation.right: searchBtn
            KeyNavigation.down: categoryBox
         }
         Label {
            Layout.fillWidth: true
            text: library.current >= 0 ? library.systems[ library.current ].name : ""
            font.pixelSize: 20
            elide: Text.ElideRight
         }
         ToolButton {
            id: searchBtn
            text: "Search"
            onClicked: searchField.forceActiveFocus()
            KeyNavigation.left: backBtn
            KeyNavigation.down: categoryBox
         }
      }
   }

   ColumnLayout {
      anchors.fill: parent
      anchors.margins: 8
      spacing: 4

      // Click focus only: the D-pad must not land here, the soft keyboard would cover the list.
      TextField {
         id: searchField
         Layout.fillWidth: true
         focusPolicy: Qt.ClickFocus
         placeholderText: "Search"
         text: games.search
         onTextEdited: games.search = text
         KeyNavigation.down: categoryBox
      }

      ComboBox {
         id: categoryBox
         Layout.fillWidth: true
         model: games.categoryNames
         currentIndex: games.category
         onActivated: ( aIndex ) => games.category = aIndex
         KeyNavigation.up: searchBtn
         KeyNavigation.down: romCheck
      }

      RowLayout {
         Layout.fillWidth: true

         CheckBox {
            id: romCheck
            text: "List by ROM"
            checked: games.listByRom
            onToggled: games.listByRom = checked
            KeyNavigation.up: categoryBox
            KeyNavigation.down: list
            KeyNavigation.right: fullCheck
         }
         CheckBox {
            id: fullCheck
            text: "Full ROM name"
            enabled: games.listByRom
            checked: games.fullRomName
            onToggled: games.fullRomName = checked
            KeyNavigation.up: categoryBox
            KeyNavigation.down: list
            KeyNavigation.left: romCheck
         }
      }

      ListView {
         id: list
         Layout.fillWidth: true
         Layout.fillHeight: true
         clip: true
         model: games.model
         KeyNavigation.up: romCheck

         delegate: ItemDelegate {
            width: ListView.view.width
            text: model.display
            highlighted: ListView.isCurrentItem
            onClicked: {
               list.currentIndex = index
               games.select( index )
            }
         }
      }

      Label {
         Layout.fillWidth: true
         text: games.countText
      }
   }
}
