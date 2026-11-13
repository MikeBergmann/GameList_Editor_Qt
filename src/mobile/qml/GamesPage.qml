import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
   id: page
   readonly property bool wide: width > height
   StackView.onActivated: list.forceActiveFocus()

   // The edit page can move to another game; the highlighted row follows.
   Connections {
      target: games
      function onCurrentChanged() {
         list.currentIndex = games.currentRow()
         list.positionViewAtIndex( list.currentIndex, ListView.Contain )
      }
   }

   Component { id: editPage; EditPage {} }
   Component { id: actionsPage; ActionsPage {} }

   header: ToolBar {
      RowLayout {
         anchors.fill: parent

         BarButton {
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
         BarButton {
            id: searchBtn
            text: "Search"
            onClicked: searchField.forceActiveFocus()
            KeyNavigation.left: backBtn
            KeyNavigation.right: actionsBtn
            KeyNavigation.down: categoryBox
         }
         BarButton {
            id: actionsBtn
            text: "Actions"
            onClicked: page.StackView.view.push( actionsPage )
            KeyNavigation.left: searchBtn
            KeyNavigation.down: categoryBox
         }
      }
   }

   // Portrait stacks filters, list and count; landscape puts the filters left of a full-height list.
   Item {
      id: body
      anchors.fill: parent
      anchors.margins: 8

      ColumnLayout {
         id: filters
         width: page.wide ? body.width / 3 : body.width
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
            KeyNavigation.right: page.wide ? list : null

            // A on the pad is Enter, which a stock ItemDelegate ignores.
            delegate: ItemDelegate {
               required property string modelData
               required property int index
               width: categoryBox.width
               text: modelData
               highlighted: categoryBox.highlightedIndex === index
               Keys.onReturnPressed: click()
               Keys.onEnterPressed: click()
            }
            // Closed, the combo would eat Up/Down and step through the categories; open, it needs them.
            Keys.onReturnPressed: popup.open()
            Keys.onEnterPressed: popup.open()
            Keys.onUpPressed: ( aEvent ) => { if ( popup.visible ) aEvent.accepted = false; else searchBtn.forceActiveFocus( Qt.TabFocusReason ) }
            Keys.onDownPressed: ( aEvent ) => { if ( popup.visible ) aEvent.accepted = false; else romCheck.forceActiveFocus( Qt.TabFocusReason ) }
         }

         GridLayout {
            Layout.fillWidth: true
            columns: page.wide ? 1 : 2
            rowSpacing: 0

            CheckBox {
               id: romCheck
               text: "List by ROM"
               checked: games.listByRom
               onToggled: games.listByRom = checked
               KeyNavigation.up: categoryBox
               KeyNavigation.down: page.wide ? fullCheck : list
               KeyNavigation.right: page.wide ? list : fullCheck
            }
            CheckBox {
               id: fullCheck
               text: "Full ROM name"
               enabled: games.listByRom
               checked: games.fullRomName
               onToggled: games.fullRomName = checked
               KeyNavigation.up: page.wide ? romCheck : categoryBox
               KeyNavigation.down: list
               KeyNavigation.left: page.wide ? null : romCheck
               KeyNavigation.right: page.wide ? list : null
            }
         }
      }

      ListView {
         id: list
         x: page.wide ? filters.width + 8 : 0
         y: page.wide ? 0 : filters.height + 4
         width: body.width - x
         height: page.wide ? body.height : body.height - y - countLabel.height - 4
         clip: true
         model: games.model
         KeyNavigation.up: page.wide ? searchBtn : romCheck
         KeyNavigation.left: page.wide ? categoryBox : null

         delegate: ItemDelegate {
            width: ListView.view.width
            // A button's text reads & as a mnemonic marker, so a literal one is doubled.
            text: model.display.replace( /&/g, "&&" )
            highlighted: ListView.isCurrentItem
            onClicked: {
               list.currentIndex = index
               games.select( index )
               page.StackView.view.push( editPage )
            }
         }
      }

      Label {
         id: countLabel
         y: body.height - height
         width: page.wide ? filters.width : body.width
         text: games.countText
      }
   }
}
