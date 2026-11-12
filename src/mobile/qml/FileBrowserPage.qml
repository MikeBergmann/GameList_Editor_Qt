import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
   id: page

   // File mode: a suffix list (without dots) makes files pickable and hides "Select this folder".
   property var suffixes: []
   readonly property bool fileMode: suffixes.length > 0
   signal picked( string aPath )

   Component.onCompleted: {
      browser.setSuffixes( suffixes )
      browser.open( fileMode && browser.path !== "" ? browser.path : library.rootPath )
      list.forceActiveFocus()
   }
   StackView.onActivated: list.forceActiveFocus()

   // B and Back go up a folder; at the list of storage roots they leave the page.
   function goUp( aEvent ) {
      if ( browser.atRoots )
         aEvent.accepted = false
      else
         browser.up()
   }
   Keys.onEscapePressed: ( aEvent ) => goUp( aEvent )
   Keys.onBackPressed: ( aEvent ) => goUp( aEvent )

   header: ToolBar {
      RowLayout {
         anchors.fill: parent

         ToolButton {
            id: upBtn
            text: "Up"
            enabled: !browser.atRoots
            onClicked: browser.up()
            KeyNavigation.down: list
         }
         Label {
            Layout.fillWidth: true
            text: browser.atRoots ? "Storage" : browser.path
            elide: Text.ElideLeft
         }
      }
   }

   ListView {
      id: list
      anchors.fill: parent
      clip: true
      model: browser.entries
      KeyNavigation.up: upBtn
      KeyNavigation.down: selectBtn

      delegate: ItemDelegate {
         width: ListView.view.width
         text: page.fileMode && modelData.isDir ? modelData.name + "/" : modelData.name
         highlighted: ListView.isCurrentItem
         onClicked: {
            list.currentIndex = index
            if ( modelData.isDir ) {
               browser.open( modelData.path )
            } else {
               page.picked( modelData.path )
               page.StackView.view.pop()
            }
         }
      }

      Label {
         anchors.centerIn: parent
         visible: list.count === 0
         text: page.fileMode ? "No folders or matching files" : "No subfolders"
      }
   }

   footer: Button {
      id: selectBtn
      text: "Select this folder"
      visible: !page.fileMode
      enabled: !browser.atRoots
      onClicked: {
         library.openRoot( browser.path )
         page.StackView.view.pop()
      }
      KeyNavigation.up: list
   }
}
