import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
  id: root

  Component.onCompleted: script.initializeUi(root)

  property real frameRate: 0.0
  property real time: 0.0
  property real timeDelta: 0.0
  property var dateParts: []

  Timer {
    interval: 16
    running: true
    repeat: true
    onTriggered: {
      var state = script.getState()
      root.frameRate = state.frameRate
      root.time = state.time
      root.timeDelta = state.timeDelta
      root.dateParts = state.date
    }
  }

  function formatSeconds(seconds) {
    var total = Math.max(0, seconds)
    var m = Math.floor(total / 60)
    var s = total - m * 60
    var w = Math.floor(s)
    var ms = Math.floor((s - w) * 1000)
    return (m < 10 ? "0" : "") + m + ":"
         + (w < 10 ? "0" : "") + w + "."
         + (ms < 100 ? "0" : "") + (ms < 10 ? "0" : "") + ms
  }

  function formatDate(parts) {
    if (!parts || parts.length < 4)
      return ""
    var year = parts[0]
    var month = parts[1]
    var day = parts[2]
    var seconds = parts[3]
    var timeText = formatSeconds(seconds)
    return year + "-" + (month < 10 ? "0" : "") + month
         + "-" + (day < 10 ? "0" : "") + day + " " + timeText
  }

  ColumnLayout {
    anchors.fill: parent
    spacing: 8

    Label {
      text: "App Time"
      font.bold: true
    }

    Label {
      text: root.formatSeconds(root.time)
      font.pixelSize: 28
      font.family: "Consolas"
    }

    Label {
      text: root.time.toFixed(3) + " s"
      font.pixelSize: 14
      font.family: "Consolas"
      opacity: 0.7
    }

    GridLayout {
      columns: 2
      rowSpacing: 6
      columnSpacing: 12
      Layout.fillWidth: true

      Label { text: "frameRate" }
      Label {
        text: root.frameRate.toFixed(3)
        font.family: "Consolas"
      }

      Label { text: "timeDelta" }
      Label {
        text: root.formatSeconds(root.timeDelta)
        font.family: "Consolas"
      }

      Label { text: "date" }
      Label {
        text: root.formatDate(root.dateParts)
        font.family: "Consolas"
        Layout.fillWidth: true
        elide: Text.ElideRight
      }
    }
  }
}
