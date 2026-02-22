"use strict"

const manifest = {
  name: "&Sliders"
}

function isExpressionEditor(editor) {
  return (typeof editor === 'string'
    && editor.startsWith('Expression'))
}

class Script {
  initializeUi(ui) {
    this.ui = ui

    const bindings = app.session.findItems((item) => {
      return (item.type === 'Binding'
        && item.bindingType === 'Uniform'
        && isExpressionEditor(item.editor))
    })

    for (let binding of bindings)
      ui.addBindingGroup(binding)
  }
}


this.script = new Script()

app.openEditor("ui.qml", manifest.name)
