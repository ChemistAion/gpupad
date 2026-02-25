import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
  id: root
  width: 320
  implicitHeight: mainLayout.implicitHeight + 16

  // ?? State ?????????????????????????????????????????????????????????????????
  property bool chR: true
  property bool chG: true
  property bool chB: true
  property bool chA: false

  // ?? Layout ????????????????????????????????????????????????????????????????
  ColumnLayout {
    id: mainLayout
    anchors.fill: parent
    spacing: 10

    // ?? Expression input ??????????????????????????????????????????????????
    Label { text: "Expression"; font.bold: true }

    RowLayout {
      Layout.fillWidth: true

      TextField {
        id: exprField
        Layout.fillWidth: true
        placeholderText: "e.g. gl_FragCoord.xy / iResolution.xy"
        font.family: "Consolas"
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
      opacity: 0.85
      text: "Enter a GLSL expression and press Apply"
    }


    // Target draw call
    Label { text: "Target Call"; font.bold: true }

    RowLayout {
      Layout.fillWidth: true
      spacing: 8

      ComboBox {
        id: callBox
        Layout.fillWidth: true
        model: []
        property var callIds: []
        property int selectedCallId: -1
        onActivated: function(index) {
          if (index >= 0 && index < callIds.length)
            selectedCallId = callIds[index]
        }
      }

      Button {
        text: "\u21BB"
        implicitWidth: 32
        ToolTip.text: "Refresh draw call list"
        ToolTip.visible: hovered
        onClicked: root.refreshCallList()
      }
    }
    // ?? Mapping ???????????????????????????????????????????????????????????
    Label { text: "Mapping"; font.bold: true }

    RowLayout {
      Layout.fillWidth: true
      spacing: 8

      Label { text: "Mode:" }
      ComboBox {
        id: modeBox
        model: ["Linear", "Sigmoid", "Log"]
        Layout.fillWidth: true
        onActivated: inspector.setMappingMode(currentIndex)
      }
    }

    RowLayout {
      Layout.fillWidth: true
      spacing: 6

      Label { text: "Min:" }
      SpinBox {
        id: rangeMin
        from: -100000
        to: 100000
        value: 0
        editable: true
        stepSize: 100
        Layout.fillWidth: true
        property real realValue: value / 1000.0
        textFromValue: function(value, locale) {
          return (value / 1000.0).toFixed(3)
        }
        valueFromText: function(text, locale) {
          return Math.round(parseFloat(text) * 1000)
        }
        onValueModified: root.pushRange()
      }

      Label { text: "Max:" }
      SpinBox {
        id: rangeMax
        from: -100000
        to: 100000
        value: 1000
        editable: true
        stepSize: 100
        Layout.fillWidth: true
        property real realValue: value / 1000.0
        textFromValue: function(value, locale) {
          return (value / 1000.0).toFixed(3)
        }
        valueFromText: function(text, locale) {
          return Math.round(parseFloat(text) * 1000)
        }
        onValueModified: root.pushRange()
      }
    }

    CheckBox {
      id: oorCheck
      text: "Highlight out-of-range"
      checked: false
      onToggled: inspector.setHighlightOOR(checked)
    }

    // ?? Channels ??????????????????????????????????????????????????????????
    Label { text: "Histogram Channels"; font.bold: true }

    RowLayout {
      Layout.fillWidth: true
      spacing: 12

      CheckBox {
        id: chkR; text: "R"; checked: root.chR
        onToggled: { root.chR = checked; root.pushChannels() }
        contentItem: Text { text: parent.text; color: "#e06060"
          leftPadding: parent.indicator.width + 4
          verticalAlignment: Text.AlignVCenter }
      }
      CheckBox {
        id: chkG; text: "G"; checked: root.chG
        onToggled: { root.chG = checked; root.pushChannels() }
        contentItem: Text { text: parent.text; color: "#40b040"
          leftPadding: parent.indicator.width + 4
          verticalAlignment: Text.AlignVCenter }
      }
      CheckBox {
        id: chkB; text: "B"; checked: root.chB
        onToggled: { root.chB = checked; root.pushChannels() }
        contentItem: Text { text: parent.text; color: "#5080e0"
          leftPadding: parent.indicator.width + 4
          verticalAlignment: Text.AlignVCenter }
      }
      CheckBox {
        id: chkA; text: "A"; checked: root.chA
        onToggled: { root.chA = checked; root.pushChannels() }
      }
    }

    // ?? Histogram height ??????????????????????????????????????????????????
    RowLayout {
      Layout.fillWidth: true
      spacing: 6

      Label { text: "Histogram height:" }
      Slider {
        id: histSlider
        Layout.fillWidth: true
        from: 0.05; to: 0.5; value: 0.2; stepSize: 0.01
        onMoved: inspector.setHistogramHeight(value)
      }
      Label {
        text: Math.round(histSlider.value * 100) + "%"
        font.family: "Consolas"
        font.pixelSize: 11
      }
    }

    // ?? Disable ???????????????????????????????????????????????????????????
    RowLayout {
      Layout.fillWidth: true

      Item { Layout.fillWidth: true }
      Button {
        text: "Disable Inspector"
        onClicked: {
          inspector.cleanup()
          statusLabel.text = "Inspector disabled"
        }
      }
    }
  }

  // ?? Helpers ?????????????????????????????????????????????????????????????

  function applyExpr() {
    const expr = exprField.text.trim()
    if (!expr) {
      statusLabel.text = "? Enter an expression first"
      return
    }
    const cid = callBox.selectedCallId > 0 ? callBox.selectedCallId : undefined
    const result = inspector.apply(expr, cid)
    if (result.ok) {
      let msg = "?  " + expr + "  ?  " + result.type
      if (result.rangeHint) {
        msg += "  [" + result.rangeHint.min + ", " + result.rangeHint.max + "]"
        rangeMin.value = Math.round(result.rangeHint.min * 1000)
        rangeMax.value = Math.round(result.rangeHint.max * 1000)
      }
      statusLabel.text = msg
    } else {
      statusLabel.text = "?  " + result.error
    }
  }

  function pushRange() {
    inspector.setMappingRange(rangeMin.realValue, rangeMax.realValue)
  }

  function pushChannels() {
    inspector.setChannelMask(root.chR, root.chG, root.chB, root.chA)
  }

  function refreshCallList() {
    const calls = inspector.listDrawCalls()
    let labels = []
    let ids = []
    for (let i = 0; i < calls.length; i++) {
      labels.push(calls[i].label)
      ids.push(calls[i].callId)
    }
    callBox.model = labels
    callBox.callIds = ids
    if (ids.length > 0 && callBox.selectedCallId <= 0) {
      callBox.currentIndex = 0
      callBox.selectedCallId = ids[0]
    }
  }

  Component.onCompleted: refreshCallList()
}
