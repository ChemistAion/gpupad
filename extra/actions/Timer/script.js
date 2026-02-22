"use strict"

const manifest = {
  name: "&Timer"
}

class Script {
  constructor() {
    this._startMs = Date.now()
    this._offset = 0.0
    this.elapsed = 0.0
    this.running = true
    this._bindings = []
  }

  initializeUi(ui) {
    this.ui = ui
    this._scanBindings()
  }

  // find uniform bindings whose expression is "app.time"
  _scanBindings() {
    this._bindings = []
    var bindings = app.session.findItems(function(item) {
      return (item.type === 'Binding'
        && item.bindingType === 'Uniform'
        && item.editor === 'Expression'
        && item.values.length === 1
        && item.values[0] === 'app.time')
    })
    for (var i = 0; i < bindings.length; ++i)
      this._bindings.push(bindings[i].id)
  }

  start() {
    if (!this.running) {
      this._startMs = Date.now()
      this.running = true
    }
  }

  stop() {
    if (this.running) {
      this._offset = this.elapsed
      this.running = false
    }
  }

  reset() {
    this._offset = 0.0
    this._startMs = Date.now()
    this.elapsed = 0.0
  }

  tick() {
    if (this.running)
      this.elapsed = this._offset + (Date.now() - this._startMs) / 1000.0

    // push to session bindings referencing "app.time"
    for (var i = 0; i < this._bindings.length; ++i) {
      var binding = app.session.findItem(this._bindings[i])
      if (binding)
        binding.values = [this.elapsed]
    }

    return this.elapsed
  }
}

this.script = new Script()

app.openEditor("ui.qml", manifest.name)
