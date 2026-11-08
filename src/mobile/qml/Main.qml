import QtQuick
import QtQuick.Controls

ApplicationWindow {
   id: win
   visible: true
   width: 420
   height: 800
   title: "GameList Editor"

   Item {
      id: root
      anchors.fill: parent

      // A on the pad is Enter, and a Button only clicks on Space.
      function activate( aItem ) {
         if ( !aItem )
            return
         if ( aItem.click )
            aItem.click()
         else if ( aItem.currentItem && aItem.currentItem.click )
            aItem.currentItem.click()
         else if ( aItem.popup )
            aItem.popup.open()
      }

      // B is Escape and the system Back key is Back; at the first page Back leaves the app.
      function goBack( aEvent ) {
         if ( content.item && content.item.depth > 1 )
            content.item.pop()
         else
            aEvent.accepted = false
      }

      Keys.onReturnPressed: ( aEvent ) => activate( win.activeFocusItem )
      Keys.onEnterPressed: ( aEvent ) => activate( win.activeFocusItem )
      Keys.onEscapePressed: ( aEvent ) => goBack( aEvent )
      Keys.onBackPressed: ( aEvent ) => goBack( aEvent )

      // Created only with access, so losing it while running drops back to the permission page.
      Loader {
         id: content
         anchors.fill: parent
         active: storage.hasAccess
         sourceComponent: StackView { initialItem: SystemsPage {} }
      }

      PermissionPage {
         anchors.fill: parent
         visible: !storage.hasAccess
      }
   }
}
