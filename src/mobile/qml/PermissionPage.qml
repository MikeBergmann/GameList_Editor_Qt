import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
   id: page

   Component.onCompleted: grant.forceActiveFocus()
   onVisibleChanged: if ( visible ) grant.forceActiveFocus()

   header: ToolBar {
      Label {
         anchors.centerIn: parent
         text: "GameList Editor"
         font.pixelSize: 20
      }
   }

   ColumnLayout {
      anchors.centerIn: parent
      width: parent.width - 32
      spacing: 16

      Label {
         Layout.fillWidth: true
         text: "GameList Editor edits gamelist.xml files, ROMs and media in your storage folders, so it needs access to all files. Without it nothing can be opened."
         wrapMode: Text.Wrap
      }
      Label {
         Layout.fillWidth: true
         text: "Press the button, switch on \"Allow access to manage all files\", then come back."
         wrapMode: Text.Wrap
      }
      Button {
         id: grant
         Layout.alignment: Qt.AlignHCenter
         text: "Open settings"
         onClicked: storage.request()
      }
   }
}
