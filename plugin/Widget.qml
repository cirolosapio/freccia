import QtQuick
import Quickshell
import Quickshell.Io
import qs.Commons
import qs.Ui
import "Model.js" as Model

// Radiofreccia in the bar. The Freccia app owns the player and writes its
// state to $XDG_RUNTIME_DIR/freccia/state.json; this widget only watches that
// file and sends commands through the freccia binary.
BarWidget {
  id: root
  moduleName: "freccia.radio"

  property var radio: Model.parseState("")
  readonly property string statePath: Quickshell.env("XDG_RUNTIME_DIR") + "/freccia/state.json"

  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight

  Translations { id: strings; directory: Qt.resolvedUrl("i18n") }

  FileView {
    id: stateFile
    path: root.statePath
    watchChanges: true
    printErrors: false
    onFileChanged: reload()
    onLoaded: root.radio = Model.parseState(text())
    onLoadFailed: root.radio = Model.parseState("")
  }

  // The app replaces the file atomically on every change. Should inotify
  // miss the new inode, this keeps the icon from going stale.
  Timer {
    interval: 3000
    running: true
    repeat: true
    onTriggered: stateFile.reload()
  }

  BarIconButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    text: "\u{F0439}"
    active: root.radio.on
    opacity: root.radio.state === "connecting" || root.radio.state === "retrying" ? 0.55 : 1
    tooltipText: Model.tooltip(root.radio, function(key) { return strings.t(key) })
    onPressed: function(b) {
      if (!root.bar) return
      if (b === Qt.RightButton) root.bar.run("freccia open")
      else root.bar.run("freccia toggle")
    }
    onWheelMoved: function(delta) {
      if (root.bar && root.radio.on) root.bar.run(delta > 0 ? "freccia volume +5" : "freccia volume -5")
    }
  }
}
