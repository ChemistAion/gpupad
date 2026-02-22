import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
  id: root

  Component.onCompleted: script.initializeUi(root)

  property real elapsed: 0.0
  property bool isRunning: true

  Timer {
    id: clock
    interval: 16
    running: true
    repeat: true
    onTriggered: root.elapsed = script.tick()
  }

  function fmt(s) {
    var m = Math.floor(s / 60)
    var sec = s - m * 60
    var w = Math.floor(sec)
    var ms = Math.floor((sec - w) * 1000)
    return (m < 10 ? "0" : "") + m + ":"
         + (w < 10 ? "0" : "") + w + "."
         + (ms < 100 ? "0" : "") + (ms < 10 ? "0" : "") + ms
  }

  ColumnLayout {
    anchors.fill: parent
    spacing: 8

    Label {
      text: "Timer"
      font.bold: true
    }

    Label {
      text: root.fmt(root.elapsed)
      font.pixelSize: 28
      font.family: "Consolas"
    }

    Label {
      text: root.elapsed.toFixed(3) + " s"
      font.pixelSize: 14
      font.family: "Consolas"
      opacity: 0.7
    }

    RowLayout {
      spacing: 4

      Button {
        text: root.isRunning ? "Stop" : "Start"
        onClicked: {
          if (root.isRunning) script.stop()
          else script.start()
          root.isRunning = !root.isRunning
        }
      }

      Button {
        text: "Reset"
        onClicked: {
          script.reset()
          root.elapsed = 0.0
        }
      }
    }

    Item { Layout.fillHeight: true }

    Label {
      text: 'Bindings with Expression <b>app.time</b> are driven by this timer.'
      textFormat: Text.RichText
      opacity: 0.5
      font.pixelSize: 11
      wrapMode: Text.WordWrap
      Layout.fillWidth: true
    }
  }
}
