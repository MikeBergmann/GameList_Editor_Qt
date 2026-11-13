import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The ScreenScraper login, scrape language and proxy. Every change is stored at once.
Page {
   id: page
   StackView.onActivated: list.forceActiveFocus( Qt.TabFocusReason )

   readonly property var rows: [
      { key: "ssUser", label: "ScreenScraper login", type: "text" },
      { key: "ssPwd", label: "ScreenScraper password", type: "secret" },
      { key: "language", label: "Scrape language", type: "language" },
      { key: "proxyUse", label: "Use a proxy", type: "flag" },
      { key: "proxyServer", label: "Proxy server", type: "text" },
      { key: "proxyPort", label: "Proxy port", type: "text" },
      { key: "proxyUser", label: "Proxy username", type: "text" },
      { key: "proxyPwd", label: "Proxy password", type: "secret" }
   ]

   function activateRow( aRow ) {
      const current = settings.values[ aRow.key ]
      if ( aRow.type === "flag" ) {
         settings.setValue( aRow.key, !current )
      } else if ( aRow.type === "language" ) {
         settings.setValue( aRow.key, ( current + 1 ) % settings.languageNames.length )
      } else {
         fieldDialog.title = aRow.label
         fieldDialog.key = aRow.key
         fieldDialog.value = current || ""
         fieldDialog.open()
      }
   }

   FieldDialog {
      id: fieldDialog
      property string key
      onCommitted: ( aText ) => settings.setValue( key, aText )
      onClosed: list.forceActiveFocus( Qt.TabFocusReason )
   }

   header: ToolBar {
      RowLayout {
         anchors.fill: parent

         BarButton {
            id: backBtn
            text: "Back"
            onClicked: page.StackView.view.pop()
            KeyNavigation.down: list
         }
         Label {
            Layout.fillWidth: true
            text: "Settings"
            font.pixelSize: 20
         }
      }
   }

   ListView {
      id: list
      anchors.fill: parent
      clip: true
      model: page.rows
      KeyNavigation.up: backBtn

      delegate: ItemDelegate {
         id: row
         width: ListView.view.width
         highlighted: ListView.isCurrentItem

         readonly property var raw: settings.values[ modelData.key ]
         readonly property string shown: modelData.type === "flag" ? ( raw ? "Yes" : "No" )
                                         : modelData.type === "language" ? settings.languageNames[ raw ]
                                         : modelData.type === "secret" ? ( raw ? "\u2022\u2022\u2022\u2022\u2022\u2022" : "" )
                                         : ( raw || "" )

         contentItem: ColumnLayout {
            spacing: 0

            Label {
               Layout.fillWidth: true
               text: modelData.label
               font.pixelSize: 12
               opacity: 0.7
            }
            Label {
               Layout.fillWidth: true
               text: row.shown.length > 0 ? row.shown : "-"
               elide: Text.ElideRight
            }
         }

         onClicked: {
            list.currentIndex = index
            page.activateRow( modelData )
         }
      }
   }
}
