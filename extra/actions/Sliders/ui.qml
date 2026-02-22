import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
  id: root
  ScrollBar.vertical.policy: ScrollBar.AlwaysOn
  ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
  contentWidth: root.availableWidth
  contentHeight: column.implicitHeight

  Component.onCompleted: script.initializeUi(root)

  ColumnLayout {
    id: column
    width: root.availableWidth
    spacing: 12
  }

  Component {
    id: groupComponent

    GroupBox {
      property string bindingName
      property int bindingId
      property string editor
      property var values: []

      title: bindingName
      Layout.fillWidth: true

      ColumnLayout {
        id: groupColumn
        anchors.fill: parent
        spacing: 4
      }

      Component.onCompleted: {
        for (var i = 0; i < values.length; ++i)
          addValueSlider(groupColumn, i)
      }

      function valueLabel(editor, index) {
        if (editor === "Expression")
          return "Value"

        // matrix editors: ExpressionAxB (A cols x B rows, column-major)
        var mat = editor.match(/^Expression(\d)x(\d)$/)
        if (mat) {
          var cols = parseInt(mat[1])
          var rows = parseInt(mat[2])
          var col = Math.floor(index / rows)
          var row = index % rows
          return "Value[" + col + "][" + row + "]"
        }

        // vector editors: Expression2..4
        return "Value[" + index + "]"
      }

      function addValueSlider(parent, index) {
        var parsed = parseFloat(values[index])
        if (isNaN(parsed))
          parsed = 0

        var margin = Math.max(0.5, Math.abs(parsed) * 0.5)
        var lo = parsed - margin
        var hi = parsed + margin

        sliderRowComponent.createObject(parent, {
          label: valueLabel(editor, index),
          bindingId: bindingId,
          valueIndex: index,
          from: lo,
          to: hi,
          value: parsed
        })
      }
    }
  }

  Component {
    id: sliderRowComponent

    RowLayout {
      property string label
      property int bindingId
      property int valueIndex
      property alias from: slider.from
      property alias to: slider.to
      property alias value: slider.value

      Label {
        text: label
        Layout.minimumWidth: 80
      }
      Slider {
        id: slider
        Layout.fillWidth: true
        onMoved: {
          var binding = app.session.findItem(bindingId)
          var vals = binding.values
          vals[valueIndex] = this.value
          binding.values = vals
        }
      }
    }
  }

  Label {
    id: emptyLabel
    text: "No Expression uniform bindings in session."
    visible: column.children.length === 0
    padding: 12
  }

  function addBindingGroup(binding) {
    emptyLabel.visible = false
    groupComponent.createObject(column, {
      bindingName: binding.name,
      bindingId: binding.id,
      editor: binding.editor,
      values: binding.values
    })
  }
}
