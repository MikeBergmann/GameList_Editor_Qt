import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The game's files, play data and hashes, read only. Cannot be closed while the hashes are computed.
Dialog {
   id: dlg
   readonly property var rows: [
      { key: "gamelist", label: "Gamelist" },
      { key: "rom", label: "ROM" },
      { key: "image", label: "Image" },
      { key: "video", label: "Video" },
      { key: "playcount", label: "Playcount" },
      { key: "lastPlayed", label: "Last played on" },
      { key: "crc32", label: "CRC32", mono: true },
      { key: "md5", label: "MD5", mono: true },
      { key: "sha1", label: "SHA1", mono: true }
   ]
   readonly property bool hashesMissing: !edit.info.crc32 || !edit.info.md5 || !edit.info.sha1

   modal: true
   parent: Overlay.overlay
   anchors.centerIn: Overlay.overlay
   width: Overlay.overlay.width - 24
   height: Math.min( implicitHeight, Overlay.overlay.height - 16 )
   closePolicy: edit.hashing ? Popup.NoAutoClose : Popup.CloseOnEscape
   onOpened: ( hashesMissing ? hashBtn : closeBtn ).forceActiveFocus( Qt.TabFocusReason )

   Connections {
      target: edit
      // The Calculate button just disabled itself under the focus.
      function onInfoChanged() {
         if ( dlg.visible && !edit.hashing )
            closeBtn.forceActiveFocus( Qt.TabFocusReason )
      }
   }

   contentItem: Flickable {
      id: flick
      clip: true
      contentHeight: column.implicitHeight
      implicitHeight: column.implicitHeight
      boundsBehavior: Flickable.StopAtBounds

      ColumnLayout {
         id: column
         width: flick.width
         spacing: 6

         Repeater {
            model: dlg.rows

            ColumnLayout {
               Layout.fillWidth: true
               spacing: 0

               Label {
                  text: modelData.label
                  font.pixelSize: 12
                  opacity: 0.7
               }
               Label {
                  Layout.fillWidth: true
                  text: edit.info[ modelData.key ] || "-"
                  wrapMode: Text.WrapAnywhere
                  font.family: modelData.mono ? "monospace" : Qt.application.font.family
               }
            }
         }
      }
   }

   footer: RowLayout {
      spacing: 8

      Button {
         id: hashBtn
         Layout.margins: 8
         text: edit.hashing ? "Calculating..." : "Calculate hashes"
         enabled: !edit.hashing && dlg.hashesMissing
         onClicked: edit.computeHashes()
         Keys.onReturnPressed: click()
         Keys.onEnterPressed: click()
         Keys.onRightPressed: closeBtn.forceActiveFocus( Qt.TabFocusReason )
         Keys.onUpPressed: flick.contentY = Math.max( 0, flick.contentY - 60 )
         Keys.onDownPressed: flick.contentY = Math.min( Math.max( 0, flick.contentHeight - flick.height ), flick.contentY + 60 )
      }
      Item { Layout.fillWidth: true }
      Button {
         id: closeBtn
         Layout.margins: 8
         text: "Close"
         enabled: !edit.hashing
         onClicked: dlg.close()
         Keys.onReturnPressed: click()
         Keys.onEnterPressed: click()
         Keys.onLeftPressed: hashBtn.forceActiveFocus( Qt.TabFocusReason )
         Keys.onUpPressed: flick.contentY = Math.max( 0, flick.contentY - 60 )
         Keys.onDownPressed: flick.contentY = Math.min( Math.max( 0, flick.contentHeight - flick.height ), flick.contentY + 60 )
      }
   }
}
