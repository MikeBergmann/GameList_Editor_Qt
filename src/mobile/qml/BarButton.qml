import QtQuick
import QtQuick.Controls

// Material's own focus ripple is a faint tint that vanishes on the blue tool bar.
ToolButton {
   background: Rectangle {
      implicitWidth: 48
      implicitHeight: 48
      radius: 4
      color: parent.down ? "#59ffffff" : parent.visualFocus ? "#40ffffff" : "transparent"
      border.width: parent.visualFocus ? 2 : 0
      border.color: "white"
   }
}
