import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The candidates of the action ActionsPage started, all ticked; the apply button writes the ticked ones.
Page {
   id: page
   StackView.onActivated: list.forceActiveFocus( Qt.TabFocusReason )

   property int picked: 0

   ListModel { id: picks }

   function fill() {
      picks.clear()
      picked = 0
      for ( const row of actions.items ) {
         picks.append( { label: row.text, usable: row.enabled, ticked: row.enabled } )
         if ( row.enabled )
            ++picked
      }
   }

   Component.onCompleted: fill()

   Connections {
      target: actions
      function onItemsChanged() { page.fill() }
      function onFinished( aOk ) {
         result.success = aOk
         result.message = aOk ? actions.report : actions.error
         result.open()
      }
   }

   function setAll( aTicked ) {
      picked = 0
      for ( let i = 0; i < picks.count; ++i ) {
         const on = aTicked && picks.get( i ).usable
         picks.setProperty( i, "ticked", on )
         if ( on )
            ++picked
      }
   }

   function usableCount() {
      let n = 0
      for ( let i = 0; i < picks.count; ++i )
         if ( picks.get( i ).usable )
            ++n
      return n
   }

   function apply() {
      const rows = []
      for ( let i = 0; i < picks.count; ++i )
         if ( picks.get( i ).ticked )
            rows.push( i )
      actions.apply( rows )
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
            enabled: !actions.busy
            onClicked: page.StackView.view.pop()
            KeyNavigation.right: allBtn
            KeyNavigation.down: list
         }
         Label {
            Layout.fillWidth: true
            text: actions.title
            font.pixelSize: 20
            elide: Text.ElideRight
         }
         BarButton {
            id: allBtn
            text: page.picked === page.usableCount() ? "None" : "All"
            enabled: !actions.busy
            onClicked: page.setAll( page.picked !== page.usableCount() )
            KeyNavigation.left: backBtn
            KeyNavigation.right: applyBtn
            KeyNavigation.down: list
         }
         BarButton {
            id: applyBtn
            text: actions.applyLabel
            enabled: page.picked > 0 && !actions.busy
            onClicked: page.apply()
            KeyNavigation.left: allBtn
            KeyNavigation.down: list
         }
      }
   }

   footer: ColumnLayout {
      spacing: 0

      ProgressBar {
         Layout.fillWidth: true
         visible: actions.busy
         from: 0
         to: Math.max( 1, actions.total )
         value: actions.progress
      }
      RowLayout {
         Label {
            Layout.fillWidth: true
            padding: 8
            text: actions.busy ? actions.progress + " of " + actions.total + " done."
                               : page.picked + " of " + picks.count + " selected."
         }
         BarButton {
            text: "Cancel"
            visible: actions.busy
            onClicked: actions.cancel()
         }
      }
   }

   ColumnLayout {
      anchors.fill: parent
      spacing: 0

      Label {
         Layout.fillWidth: true
         padding: 8
         text: actions.intro
         wrapMode: Text.Wrap
         font.pixelSize: 13
         opacity: 0.8
      }
      CheckBox {
         id: choiceBox
         Layout.fillWidth: true
         visible: actions.choiceLabel !== ""
         enabled: !actions.busy
         text: actions.choiceLabel.replace( /&/g, "&&" )
         checked: actions.choice
         onToggled: actions.choice = checked
         KeyNavigation.up: backBtn
         KeyNavigation.down: list
      }

      ListView {
         id: list
         Layout.fillWidth: true
         Layout.fillHeight: true
         clip: true
         enabled: !actions.busy
         model: picks
         KeyNavigation.up: choiceBox.visible ? choiceBox : backBtn

         delegate: CheckDelegate {
            required property int index
            required property string label
            required property bool usable
            required property bool ticked

            width: ListView.view.width
            text: label.replace( /&/g, "&&" )
            enabled: usable
            // Wraps, so a long row keeps its trailing "(not renamed: ...)" reason visible.
            contentItem: Label {
               rightPadding: parent.indicator.width + parent.spacing
               text: parent.text
               wrapMode: Text.Wrap
               verticalAlignment: Text.AlignVCenter
               color: parent.palette.text
            }
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
}
