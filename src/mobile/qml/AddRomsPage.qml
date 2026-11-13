import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The ROMs the gamelist lacks, all ticked; Add writes the ticked ones.
Page {
   id: page
   StackView.onActivated: list.forceActiveFocus( Qt.TabFocusReason )

   property int picked: 0

   ListModel { id: picks }

   Component.onCompleted: {
      for ( const rom of actions.roms )
         picks.append( { rom: rom, ticked: true } )
      picked = picks.count
   }

   function setAll( aTicked ) {
      for ( let i = 0; i < picks.count; ++i )
         picks.setProperty( i, "ticked", aTicked )
      picked = aTicked ? picks.count : 0
   }

   function add() {
      const rows = []
      for ( let i = 0; i < picks.count; ++i )
         if ( picks.get( i ).ticked )
            rows.push( i )

      const ok = actions.addRoms( rows )
      result.success = ok
      result.message = ok ? actions.added + " game(s) added." : actions.error
      result.open()
   }

   AskDialog {
      id: result
      property bool success: false
      choices: [ "OK" ]
      onClosed: {
         if ( success )
            page.StackView.view.pop()
         else
            list.forceActiveFocus( Qt.TabFocusReason )
      }
   }

   header: ToolBar {
      RowLayout {
         anchors.fill: parent

         BarButton {
            id: backBtn
            text: "Back"
            onClicked: page.StackView.view.pop()
            KeyNavigation.right: allBtn
            KeyNavigation.down: list
         }
         Label {
            Layout.fillWidth: true
            text: "Add missing ROMs"
            font.pixelSize: 20
            elide: Text.ElideRight
         }
         BarButton {
            id: allBtn
            text: page.picked === picks.count ? "None" : "All"
            onClicked: page.setAll( page.picked !== picks.count )
            KeyNavigation.left: backBtn
            KeyNavigation.right: addBtn
            KeyNavigation.down: list
         }
         BarButton {
            id: addBtn
            text: "Add"
            enabled: page.picked > 0
            onClicked: page.add()
            KeyNavigation.left: allBtn
            KeyNavigation.down: list
         }
      }
   }

   footer: Label {
      padding: 8
      text: page.picked + " of " + picks.count + " selected."
   }

   ListView {
      id: list
      anchors.fill: parent
      clip: true
      model: picks
      KeyNavigation.up: backBtn

      delegate: CheckDelegate {
         required property int index
         required property string rom
         required property bool ticked

         width: ListView.view.width
         text: rom.replace( /&/g, "&&" )
         highlighted: ListView.isCurrentItem
         // Not checkable: a click would break the binding, and "All" / "None" must still reach the box.
         checkable: false
         checked: ticked

         onClicked: {
            const now = !ticked
            list.currentIndex = index
            picks.setProperty( index, "ticked", now )
            page.picked += now ? 1 : -1
         }
      }
   }
}
