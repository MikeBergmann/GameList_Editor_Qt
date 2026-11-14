import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The actions on the open system. Each one shows a review list before it writes anything.
// The row order is ActionsController::Action.
Page {
   id: page
   StackView.onActivated: list.forceActiveFocus( Qt.TabFocusReason )

   readonly property var rows: [
      { label: "Add missing ROMs", hint: "Games in the system folder that the gamelist does not list" },
      { label: "Link existing media", hint: "Pictures and videos on disk that no game links yet" },
      { label: "Rename media after their ROM", hint: "Give each linked file the name of its ROM" },
      { label: "Shrink large pictures", hint: "Re-encode pictures over a size limit, in place" },
      { label: "Delete orphans", hint: "Remove games whose ROM is missing from the gamelist" },
      { label: "Delete duplicates", hint: "Remove games that list a ROM twice" }
   ]

   Component { id: reviewPage; ReviewPage {} }

   function run( aAction, aMegabytes ) {
      if ( !actions.find( aAction, aMegabytes ) ) {
         notice.message = actions.error
         notice.open()
      } else if ( actions.items.length === 0 && actions.choiceLabel === "" ) {
         notice.message = actions.emptyText
         notice.open()
      } else {
         page.StackView.view.push( reviewPage )
      }
   }

   AskDialog {
      id: notice
      choices: [ "OK" ]
      onClosed: list.forceActiveFocus( Qt.TabFocusReason )
   }

   FieldDialog {
      id: sizeDlg
      title: "Size limit in MB"
      value: "1.0"
      onCommitted: ( aText ) => {
         const mb = parseFloat( aText.replace( ",", "." ) )
         if ( mb > 0 ) {
            page.run( 3, mb )
         } else {
            notice.message = "Enter a size above 0."
            notice.open()
         }
      }
      onClosed: list.forceActiveFocus( Qt.TabFocusReason )
   }

   header: ToolBar {
      RowLayout {
         anchors.fill: parent

         BarButton {
            id: backBtn
            text: "Back"
            onClicked: page.StackView.view.pop()
            KeyNavigation.down: list
         }
         Label {
            Layout.fillWidth: true
            text: "Actions"
            font.pixelSize: 20
         }
      }
   }

   ListView {
      id: list
      anchors.fill: parent
      clip: true
      model: page.rows
      KeyNavigation.up: backBtn

      delegate: ItemDelegate {
         width: ListView.view.width
         highlighted: ListView.isCurrentItem

         contentItem: ColumnLayout {
            spacing: 0

            Label {
               Layout.fillWidth: true
               text: modelData.label
            }
            Label {
               Layout.fillWidth: true
               text: modelData.hint
               font.pixelSize: 12
               opacity: 0.7
               wrapMode: Text.Wrap
            }
         }

         onClicked: {
            list.currentIndex = index
            if ( index === 3 )
               sizeDlg.open()
            else
               page.run( index, 1.0 )
         }
      }
   }
}
