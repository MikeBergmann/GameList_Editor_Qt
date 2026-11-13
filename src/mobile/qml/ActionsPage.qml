import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The actions on the open system. Each one shows a review list before it writes anything.
Page {
   id: page
   StackView.onActivated: list.forceActiveFocus( Qt.TabFocusReason )

   readonly property var rows: [
      { label: "Add missing ROMs", hint: "Games in the system folder that the gamelist does not list" }
   ]

   Component { id: addRomsPage; AddRomsPage {} }

   function run( aRow ) {
      if ( aRow === 0 ) {
         if ( !actions.findMissingRoms() ) {
            notice.message = actions.error
            notice.open()
         } else if ( actions.roms.length === 0 ) {
            notice.message = "Every ROM in this folder is already in the gamelist."
            notice.open()
         } else {
            page.StackView.view.push( addRomsPage )
         }
      }
   }

   AskDialog {
      id: notice
      choices: [ "OK" ]
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
            page.run( index )
         }
      }
   }
}
