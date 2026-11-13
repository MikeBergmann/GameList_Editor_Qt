import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

// Plays the game's video full screen on black. The player only exists while the popup is open, so a replaced
// file is always reloaded. Tap or A pauses (or, on an error, opens the video in another app); Close or Back leaves.
Popup {
   id: dlg
   modal: true
   parent: Overlay.overlay
   x: 0
   y: 0
   width: Overlay.overlay.width
   height: Overlay.overlay.height
   padding: 0
   closePolicy: Popup.CloseOnEscape
   onOpened: loader.item.forceActiveFocus()

   background: Rectangle { color: "black" }

   contentItem: Loader {
      id: loader
      active: dlg.visible
      sourceComponent: Item {
         property bool openFailed: false
         property bool noPicture: false
         // Qt may skip or fail to decode a video track without any error: nothing loads, or no frame arrives.
         readonly property bool failed: noPicture || player.error !== MediaPlayer.NoError
                                        || player.mediaStatus === MediaPlayer.InvalidMedia
                                        || ( !player.hasVideo && player.mediaStatus >= MediaPlayer.LoadedMedia
                                             && player.mediaStatus <= MediaPlayer.EndOfMedia )
         onFailedChanged: if ( failed ) player.stop()
         function openElsewhere() {
            if ( edit.openVideoExternally() )
               dlg.close()
            else
               openFailed = true
         }
         function toggle() {
            if ( failed )
               openElsewhere()
            else if ( player.playbackState === MediaPlayer.PlayingState )
               player.pause()
            else
               player.play()
         }
         Keys.onReturnPressed: toggle()
         Keys.onEnterPressed: toggle()
         Keys.onSpacePressed: toggle()

         // A stream Qt cannot decode reports no error: it "plays" with the position stuck at 0.
         Timer {
            interval: 4000
            running: true
            onTriggered: if ( player.playbackState === MediaPlayer.PlayingState && player.position <= 0 )
               noPicture = true
         }

         MediaPlayer {
            id: player
            source: edit.videoSource
            videoOutput: output
            loops: MediaPlayer.Infinite
            Component.onCompleted: play()
         }

         VideoOutput {
            id: output
            anchors.fill: parent
            fillMode: VideoOutput.PreserveAspectFit

            MouseArea {
               anchors.fill: parent
               onClicked: toggle()
            }
         }

         ColumnLayout {
            anchors.centerIn: parent
            width: parent.width - 48
            visible: failed
            spacing: 16

            Label {
               Layout.fillWidth: true
               horizontalAlignment: Text.AlignHCenter
               text: {
                  const codec = edit.videoCodec()
                  const detail = openFailed ? "No app could open it either." : player.errorString
                  return "This device cannot play this video" + ( codec ? " (" + codec + ")" : "" ) + "."
                         + ( detail ? " " + detail : "" )
               }
               color: "#ff8a80"
               wrapMode: Text.Wrap
            }
            Button {
               Layout.alignment: Qt.AlignHCenter
               text: "Open in another app"
               focusPolicy: Qt.NoFocus
               onClicked: openElsewhere()
            }
         }

         Button {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 8
            text: "Close"
            focusPolicy: Qt.NoFocus
            onClicked: dlg.close()
         }
      }
   }
}
