import QtQuick 2.7
import QtQuick.Controls 2.13
import QtQuick.Layouts 1.3

// Send-message panel for a running worker. Shows known fields (from live tags and the
// worker's config), each with its current live value, an editable input, and a Send
// button. A custom-JSON section at the bottom accepts arbitrary messages.
//
// Embed inside a DockablePanel; the host wires workerName, workerType, context,
// liveValues, liveQuality, and runnerLive.
Item {
    id: sender

    property string workerName: ""
    property string workerType: ""
    property var context: null
    property var liveValues: ({})
    property var liveQuality: ({})
    property bool runnerLive: false

    implicitWidth: 280
    implicitHeight: fieldCol.implicitHeight + 16

    // ── field list (merged from live tags + config-derived) ────────────────
    // each entry: { field: string, value: var, quality: string, source: "live"|"config" }
    property var fields: {
        var map = ({})
        var prefix = workerName + ":"

        // 1) live tags for this worker
        for (var key in liveValues) {
            if (key.indexOf(prefix) !== 0) continue
            var f = key.substring(prefix.length)
            map[f] = { field: f, value: liveValues[key],
                       quality: liveQuality[key] || "good", source: "live" }
        }

        // 2) derive from the worker's config (Modbus registers, etc.)
        var obj = context ? context.objects[workerName] : undefined
        if (obj && (obj.type === "ModbusMaster" || obj.type === "ModbusSlave")) {
            var regs = (obj.config || {}).registers || {}
            for (var group in regs) {
                var g = regs[group]
                if (g && typeof g === "object")
                    for (var fieldName in g)
                        if (map[fieldName] === undefined)
                            map[fieldName] = { field: fieldName, value: undefined,
                                               quality: "comm_fail", source: "config" }
            }
        }

        var out = []
        for (var k in map) out.push(map[k])
        out.sort(function(a, b) { return a.field < b.field ? -1 : 1 })
        return out
    }

    onWorkerNameChanged: customText.text = ""

    function sendField(field, value) {
        var msg = {}
        msg[field] = value
        radapter.model.send({ send_to: workerName, msg: msg })
        radapter.note("sender:field_sent|" + workerName + "|" + field)
    }

    function sendCustom() {
        var raw = customText.text.trim()
        if (raw.length === 0) return
        var obj
        try {
            obj = JSON.parse(raw)
        } catch (e) {
            customError.text = "Invalid JSON: " + e.message
            return
        }
        customError.text = ""
        radapter.model.send({ send_to: workerName, msg: obj })
        radapter.note("sender:custom_sent|" + workerName)
    }

    function fieldValue(f) {
        return f.value !== undefined && f.value !== null ? String(f.value) : ""
    }

    ColumnLayout {
        id: fieldCol
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6
        visible: true

        // header
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: sender.workerName
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Rectangle {
                radius: 3
                color: "#e0e0e0"
                implicitWidth: typeLabel.implicitWidth + 8
                implicitHeight: 18
                Label {
                    id: typeLabel
                    anchors.centerIn: parent
                    text: sender.workerType
                    font.pixelSize: 10
                    color: "#555"
                }
            }
        }

        // field rows
        Label {
            visible: sender.fields.length === 0
            text: "No known fields yet — data will appear as it flows"
            color: "#999"
            font.pixelSize: 11
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: sender.fields
            delegate: Frame {
                Layout.fillWidth: true
                padding: 6
                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: 4

                    // field name + live value
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Label {
                            text: modelData.field
                            font.pixelSize: 11
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: modelData.value !== undefined ? String(modelData.value) : "—"
                            font.pixelSize: 10
                            color: modelData.quality === "comm_fail" ? "#c62828" : "#666"
                            elide: Text.ElideRight
                            Layout.preferredWidth: 60
                        }
                    }

                    // value input + send button
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        TextField {
                            id: valInput
                            Layout.fillWidth: true
                            font.pixelSize: 11
                            placeholderText: "new value"
                            selectByMouse: true
                        }
                        Button {
                            text: "Send"
                            implicitWidth: 44
                            implicitHeight: 24
                            font.pixelSize: 10
                            padding: 2
                            onClicked: {
                                var raw = valInput.text.trim()
                                if (raw.length === 0) return
                                var num = Number(raw)
                                sender.sendField(modelData.field,
                                                 isNaN(num) || raw.match(/^0[xX]/) ? raw : num)
                                valInput.text = ""
                            }
                        }
                    }
                }
            }
        }

        // ── custom JSON ─────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: "#ddd"
            Layout.topMargin: 4
            Layout.bottomMargin: 2
        }

        Label {
            text: "Custom message (JSON)"
            font.pixelSize: 11
            font.bold: true
            color: "#666"
        }

        TextArea {
            id: customText
            Layout.fillWidth: true
            Layout.preferredHeight: 80
            font.family: "monospace"
            font.pixelSize: 11
            placeholderText: "{\"key\": value}"
            selectByMouse: true
            wrapMode: TextEdit.Wrap
        }

        Label {
            id: customError
            visible: text.length > 0
            color: "#c62828"
            font.pixelSize: 10
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        Button {
            text: "Send custom"
            Layout.alignment: Qt.AlignRight
            implicitHeight: 24
            font.pixelSize: 10
            onClicked: sender.sendCustom()
        }
    }

}
