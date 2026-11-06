import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 420
    height: 360
    minimumWidth: 340
    minimumHeight: 320
    visible: false
    title: "Freccia"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool helpVisible: false

    readonly property string stateText: {
        translator.language // re-render on a language switch
        switch (radio.state) {
        case "playing": return qsTr("On air")
        case "connecting": return qsTr("Connecting…")
        case "retrying": return qsTr("Can't connect, retrying…")
        default: return qsTr("Off")
        }
    }

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "Esc"; context: Qt.ApplicationShortcut
        onActivated: win.helpVisible ? win.helpVisible = false : win.close() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; onActivated: radio.toggle() }
    Shortcut { sequence: "Up"; context: Qt.ApplicationShortcut; onActivated: radio.changeVolume(5) }
    Shortcut { sequence: "Down"; context: Qt.ApplicationShortcut; onActivated: radio.changeVolume(-5) }
    Shortcut { sequence: "M"; context: Qt.ApplicationShortcut; onActivated: radio.muted = !radio.muted }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 340)
            spacing: 14

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: radio.station
                font.pixelSize: 26
                font.weight: Font.DemiBold
                color: "#f2f2f2"
            }

            // The main action: a true circle, so it keeps its own radius.
            AbstractButton {
                id: power
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 112
                Layout.preferredHeight: 112
                focusPolicy: Qt.NoFocus
                onClicked: radio.toggle()
                Accessible.name: radio.on ? qsTr("Switch off") : qsTr("Switch on")

                background: Rectangle {
                    id: ring
                    radius: width / 2
                    color: radio.on ? theme.accent : "transparent"
                    border.width: radio.on ? 0 : 3
                    border.color: radio.on ? theme.accent : (power.hovered ? "#8a8a8f" : "#4a4a50")
                    opacity: power.pressed ? 0.8 : 1
                    Behavior on color { ColorAnimation { duration: 150 } }

                    // Pulses gently while the stream is still connecting.
                    SequentialAnimation on scale {
                        running: radio.state === "connecting" || radio.state === "retrying"
                        loops: Animation.Infinite
                        onRunningChanged: if (!running) ring.scale = 1
                        NumberAnimation { to: 0.96; duration: 500; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1.0; duration: 500; easing.type: Easing.InOutSine }
                    }
                }
                contentItem: Item {
                    // A power symbol: an open ring and a stem.
                    Canvas {
                        id: glyph
                        anchors.centerIn: parent
                        width: 44; height: 44
                        property color ink: radio.on ? theme.accentForeground : "#d8d8dc"
                        onInkChanged: requestPaint()
                        onPaint: {
                            const c = getContext("2d")
                            c.reset()
                            c.strokeStyle = ink
                            c.lineWidth = 4
                            c.lineCap = "round"
                            c.beginPath()
                            c.arc(22, 24, 16, -Math.PI / 2 + 0.6, 3 * Math.PI / 2 - 0.6)
                            c.stroke()
                            c.beginPath()
                            c.moveTo(22, 4); c.lineTo(22, 22)
                            c.stroke()
                        }
                    }
                }
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: win.stateText
                font.pixelSize: 16
                color: radio.state === "playing" ? theme.accent : (radio.state === "retrying" ? "#ff8a80" : "#b8b8bd")
            }

            Label {
                Layout.fillWidth: true
                visible: text !== ""
                text: radio.state === "retrying" ? radio.error : [radio.show, radio.title].filter(s => s !== "").join("\n")
                textFormat: Text.PlainText
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                maximumLineCount: 3
                elide: Text.ElideRight
                font.pixelSize: 13
                color: "#8a8a8f"
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                ToolButton {
                    focusPolicy: Qt.NoFocus
                    onClicked: radio.muted = !radio.muted
                    contentItem: Label {
                        text: radio.muted ? "\u{F075F}" : "\u{F057E}"
                        font.family: "JetBrainsMono Nerd Font"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 18
                        color: "#d8d8dc"
                        opacity: radio.muted ? 0.5 : 0.9
                    }
                    Accessible.name: radio.muted ? qsTr("Unmute") : qsTr("Mute")
                }
                Slider {
                    Layout.fillWidth: true
                    focusPolicy: Qt.NoFocus
                    from: 0; to: 100; stepSize: 1
                    value: radio.volume
                    enabled: !radio.muted
                    onMoved: radio.volume = value
                }
                Label {
                    Layout.preferredWidth: 44
                    horizontalAlignment: Text.AlignRight
                    text: { translator.language; return qsTr("%1%").arg(radio.volume.toLocaleString(Qt.locale(), 'f', 0)) }
                    color: "#8a8a8f"
                    font.pixelSize: 13
                }
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Press ? for shortcuts")
                font.pixelSize: 11
                color: "#5c5c62"
            }
        }

        // Every shortcut, toggled with ?.
        Rectangle {
            anchors.fill: parent
            visible: win.helpVisible
            color: "#e60e0e10"

            MouseArea { anchors.fill: parent; onClicked: win.helpVisible = false }

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - 32, help.implicitWidth + 48)
                height: help.implicitHeight + 40
                radius: theme.radius
                color: "#1a1a1e"
                border.color: "#2c2c32"

                GridLayout {
                    id: help
                    anchors.centerIn: parent
                    columns: 2
                    columnSpacing: 20
                    rowSpacing: 8

                    Repeater {
                        model: [
                            ["Space", QT_TR_NOOP("Switch the radio on or off")],
                            ["↑ ↓", QT_TR_NOOP("Volume")],
                            ["M", QT_TR_NOOP("Mute")],
                            ["Esc", QT_TR_NOOP("Hide the window, keep playing")],
                            ["Q", QT_TR_NOOP("Quit and stop the radio")],
                            ["?", QT_TR_NOOP("Show or hide shortcuts")]
                        ]
                        delegate: Item {
                            required property var modelData
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            implicitWidth: row.implicitWidth
                            implicitHeight: row.implicitHeight
                            RowLayout {
                                id: row
                                anchors.fill: parent
                                spacing: 20
                                Label {
                                    Layout.preferredWidth: 56
                                    text: modelData[0]
                                    font.family: "monospace"
                                    font.pixelSize: 13
                                    color: theme.accent
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: { translator.language; return qsTr(modelData[1]) }
                                    font.pixelSize: 13
                                    color: "#e0e0e4"
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
