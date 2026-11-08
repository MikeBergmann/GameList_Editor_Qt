// Qt drops gamepad buttons (they reach QML as Key_unknown), so re-send them as keys it maps.

import QtQuick
import QtQuick.Controls

ApplicationWindow {
   id: win
   visible: true
   title: "GLE input spike"

   property int clicks: 0
   property string lastKey: "-"

   FocusScope {
      anchors.fill: parent
      focus: true
      Keys.onPressed: ( event ) => win.lastKey = "0x" + event.key.toString( 16 ) + " '" + event.text + "'"

      // Buttons ignore Enter/Return (they only click on Space), so A has to click the focused item.
      Keys.onReturnPressed: ( event ) => { if ( win.activeFocusItem && win.activeFocusItem.click ) win.activeFocusItem.click() }
      Keys.onEnterPressed: ( event ) => { if ( win.activeFocusItem && win.activeFocusItem.click ) win.activeFocusItem.click() }

      Column {
         anchors.centerIn: parent
         spacing: 12

         Label { text: "last key: " + win.lastKey }
         Label { text: "focus: " + win.activeFocusItem + "  clicks: " + win.clicks }
         Label { text: "size: " + win.width + "x" + win.height }

         Button {
            id: first
            text: "One"
            focus: true
            onClicked: win.clicks++
            KeyNavigation.down: second
         }
         Button {
            id: second
            text: "Two"
            onClicked: win.clicks++
            KeyNavigation.up: first
            KeyNavigation.down: field
         }
         TextField {
            id: field
            placeholderText: "text field (keyboard test)"
            KeyNavigation.up: second
         }
      }
   }
}
