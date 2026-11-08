import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
   id: page

   Component.onCompleted: {
      browser.open( library.rootPath )
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
         text: modelData.name
         highlighted: ListView.isCurrentItem
         onClicked: {
            list.currentIndex = index
            browser.open( modelData.path )
         }
      }

      Label {
         anchors.centerIn: parent
         visible: list.count === 0
         text: "No subfolders"
      }
   }

   footer: Button {
      id: selectBtn
      text: "Select this folder"
      enabled: !browser.atRoots
      onClicked: {
         library.openRoot( browser.path )
         page.StackView.view.pop()
      }
      KeyNavigation.up: list
   }
}
