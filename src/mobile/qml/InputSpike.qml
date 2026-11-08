// Phase 4 input spike: does D-pad / A / B navigate and activate with plain Qt Quick focus?
// The raw keys are logged by the C++ event filter in main_android.cpp (adb logcat | findstr GLE).

import QtQuick
import QtQuick.Controls

ApplicationWindow {
   id: win
   visible: true
   title: "GLE input spike"

   property int clicks: 0

   Column {
      anchors.centerIn: parent
      spacing: 12

      Label { text: "D-pad, A, B, touch; keys go to logcat" }
      Label { text: "clicks: " + win.clicks + "  size: " + win.width + "x" + win.height }

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
