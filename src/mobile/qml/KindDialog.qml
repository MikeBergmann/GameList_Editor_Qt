import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Which system a folder the app does not know should be treated as; picked() carries an index
// into library.kindLabels (0 = no mapping).
Dialog {
   id: dlg
   property string folder
   property int current: 0
   signal picked( int aChoice )

   modal: true
   title: "Set system type"
   anchors.centerIn: Overlay.overlay
   width: Math.min( 360, Overlay.overlay.width - 32 )
   height: Math.min( 480, Overlay.overlay.height - 32 )
   closePolicy: Popup.CloseOnEscape
   onOpened: {
      kinds.currentIndex = current
      kinds.positionViewAtIndex( current, ListView.Center )
      kinds.forceActiveFocus( Qt.TabFocusReason )
   }

   contentItem: ColumnLayout {
      spacing: 8

      Label {
         Layout.fillWidth: true
         text: "Treat the folder \"" + dlg.folder + "\" as:"
         wrapMode: Text.Wrap
      }
      ListView {
         id: kinds
         Layout.fillWidth: true
         Layout.fillHeight: true
         clip: true
         model: library.kindLabels
         // A popup's keys do not reach the window's handler, so A is handled here.
         Keys.onReturnPressed: if ( currentItem ) currentItem.click()
         Keys.onEnterPressed: if ( currentItem ) currentItem.click()

         delegate: ItemDelegate {
            width: ListView.view.width
            text: modelData
            highlighted: ListView.isCurrentItem
            onClicked: {
               dlg.close()
               dlg.picked( index )
            }
         }
      }
   }
}
