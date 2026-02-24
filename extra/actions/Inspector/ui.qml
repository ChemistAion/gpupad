import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
  id: root
  width: 360
  height: 120

  ColumnLayout {
    anchors.fill: parent
    anchors.margins: 8
    spacing: 6

    RowLayout {
      Layout.fillWidth: true

      Label {
        text: "Expr:"
      }

      TextField {
        id: exprField
        Layout.fillWidth: true
        placeholderText: "e.g. gl_FragCoord.x / iResolution.x"
        font.family: "Monospace"
        font.pixelSize: 12
        onAccepted: root.applyExpr()
      }

      Button {
        text: "Apply"
        onClicked: root.applyExpr()
      }
    }

    Label {
      id: statusLabel
      Layout.fillWidth: true
      wrapMode: Text.Wrap
      font.pixelSize: 11
      text: "Enter a GLSL expression and press Apply or Enter"
    }

    RowLayout {
      Layout.fillWidth: true
      Item { Layout.fillWidth: true }
      Button {
        text: "Disable"
        onClicked: {
          inspector.cleanup()
          statusLabel.text = "Inspector disabled"
        }
      }
    }
  }

  function applyExpr() {
    const expr = exprField.text.trim()
    if (!expr) {
      statusLabel.text = "⚠ Enter an expression first"
      return
    }
    const result = inspector.apply(expr)
    statusLabel.text = result.ok
      ? "✓  " + expr + "  →  " + result.type
      : "✗  " + result.error
  }
}
