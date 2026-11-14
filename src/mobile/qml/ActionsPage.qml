import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The actions on the open system. Most show a review list before they write anything.
// The first rows are in ActionsController::Action order; Export has no review.
Page {
   id: page
   StackView.onActivated: list.forceActiveFocus( Qt.TabFocusReason )

   readonly property var rows: [
      { label: "Add missing ROMs", hint: "Games in the system folder that the gamelist does not list" },
      { label: "Link existing media", hint: "Pictures and videos on disk that no game links yet" },
      { label: "Rename media after their ROM", hint: "Give each linked file the name of its ROM" },
      { label: "Shrink large pictures", hint: "Re-encode pictures over a size limit, in place" },
      { label: "Delete orphans", hint: "Remove games whose ROM is missing from the gamelist" },
      { label: "Delete duplicates", hint: "Remove games that list a ROM twice" },
      { label: "Clean up backups", hint: "Delete old gamelist backups, keeping the newest few" },
      { label: "Export games list", hint: "Write the names now shown to a .txt file in the system folder" }
   ]

   Component { id: reviewPage; ReviewPage {} }

   function run( aAction, aValue ) {
      if ( !actions.find( aAction, aValue ) ) {
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

   FieldDialog {
      id: keepDlg
      title: "Backups to keep"
      value: "3"
      onCommitted: ( aText ) => {
         const keep = parseInt( aText )
         if ( keep >= 0 ) {
            page.run( 6, keep )
         } else {
            notice.message = "Enter a number, 0 or more."
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
            if ( index === 3 ) {
               sizeDlg.open()
            } else if ( index === 6 ) {
               keepDlg.open()
            } else if ( index === 7 ) {
               actions.exportList()
               notice.message = actions.error !== "" ? actions.error : actions.report
               notice.open()
            } else {
               page.run( index, 1.0 )
            }
         }
      }
   }
}
