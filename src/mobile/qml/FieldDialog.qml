import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Edits one text value; committed() carries the new text. Enter confirms a single-line value,
// and Start (the pad's Menu key) confirms any.
Dialog {
   id: dlg
   property string value
   property bool multiline: false
   signal committed( string aText )

   modal: true
   parent: Overlay.overlay
   x: 12
   y: 8
   width: Overlay.overlay.width - 24
   closePolicy: Popup.CloseOnEscape
   standardButtons: Dialog.Ok | Dialog.Cancel

   onOpened: {
      field.text = dlg.value
      field.forceActiveFocus()
      field.selectAll()
   }
   onAccepted: committed( field.text )

   contentItem: TextArea {
      id: field
      implicitHeight: dlg.multiline ? 160 : 56
      wrapMode: TextEdit.Wrap
      inputMethodHints: Qt.ImhNoPredictiveText

      Keys.onPressed: ( aEvent ) => {
         const enter = aEvent.key === Qt.Key_Return || aEvent.key === Qt.Key_Enter
         if ( ( enter && !dlg.multiline ) || aEvent.key === Qt.Key_Menu ) {
            aEvent.accepted = true
            dlg.accept()
         }
      }
   }
}
