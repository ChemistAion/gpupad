"use strict"

const manifest = {
  name: "&Timer"
}

class Script {
  initializeUi(ui) {
    this.ui = ui
  }

  getState() {
    return {
      frameRate: app.frameRate,
      time: app.time,
      timeDelta: app.timeDelta,
      date: app.date
    }
  }
}

this.script = new Script()

app.openEditor("ui.qml", manifest.name)
