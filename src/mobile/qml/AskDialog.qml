import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A question with one button per choice; chosen() carries the index of the one pressed.
Dialog {
   id: dlg
   property string message
   property var choices: []
   signal chosen( int aChoice )

   modal: true
   anchors.centerIn: Overlay.overlay
   width: Math.min( 360, Overlay.overlay.width - 32 )
   closePolicy: Popup.CloseOnEscape
   onOpened: buttons.itemAt( 0 ).forceActiveFocus( Qt.TabFocusReason )

   contentItem: ColumnLayout {
      spacing: 16

      Label {
         Layout.fillWidth: true
         text: dlg.message
         wrapMode: Text.Wrap
      }
      RowLayout {
         Layout.alignment: Qt.AlignRight

         Repeater {
            id: buttons
            model: dlg.choices

            Button {
               text: modelData
               onClicked: {
                  dlg.close()
                  dlg.chosen( index )
               }
               // A popup's keys do not reach the window's handler, so A and the D-pad are handled here.
               Keys.onReturnPressed: click()
               Keys.onEnterPressed: click()
               Keys.onLeftPressed: { const other = buttons.itemAt( index - 1 ); if ( other ) other.forceActiveFocus( Qt.TabFocusReason ) }
               Keys.onRightPressed: { const other = buttons.itemAt( index + 1 ); if ( other ) other.forceActiveFocus( Qt.TabFocusReason ) }
            }
         }
      }
   }
}
