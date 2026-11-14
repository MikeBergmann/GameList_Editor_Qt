import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A scrolling page of rich text. Up / Down and L1 / R1 scroll it, and B or Back leaves it.
Page {
   id: page
   property string html

   StackView.onActivated: flick.forceActiveFocus( Qt.TabFocusReason )

   header: ToolBar {
      RowLayout {
         anchors.fill: parent

         BarButton {
            id: backBtn
            text: "Back"
            onClicked: page.StackView.view.pop()
            KeyNavigation.down: flick
         }
         Label {
            Layout.fillWidth: true
            text: page.title
            font.pixelSize: 20
         }
      }
   }

   Flickable {
      id: flick
      anchors.fill: parent
      clip: true
      contentHeight: body.implicitHeight + 16
      boundsBehavior: Flickable.StopAtBounds
      ScrollBar.vertical: ScrollBar {}

      function scrollBy( aDelta ) {
         contentY = Math.max( 0, Math.min( contentHeight - height, contentY + aDelta ) )
      }

      Keys.onUpPressed: contentY <= 0 ? backBtn.forceActiveFocus( Qt.TabFocusReason ) : scrollBy( -60 )
      Keys.onDownPressed: scrollBy( 60 )
      Keys.onPressed: ( aEvent ) => {
         if ( aEvent.key === Qt.Key_PageUp || aEvent.key === Qt.Key_PageDown ) {
            aEvent.accepted = true
            scrollBy( ( aEvent.key === Qt.Key_PageUp ? -0.9 : 0.9 ) * height )
         }
      }

      Label {
         id: body
         x: 8
         y: 8
         width: flick.width - 16
         textFormat: Text.RichText
         wrapMode: Text.Wrap
         text: page.html
         onLinkActivated: ( aLink ) => Qt.openUrlExternally( aLink )
      }
   }
}
