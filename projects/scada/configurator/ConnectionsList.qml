import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

// The pipes touching `worker` in `context`, each shown with its direction (→ out,
// ← in), the other endpoint and any directive (wrap/unwrap/on), plus a remove
// button. Updates live via context.revision. Shown beside the WorkerConfigurator
// when a node is selected, so a worker's connections are editable in place.
ColumnLayout {
    id: panel
    spacing: 4

    property var context: null
    property string worker: ""

    // pipes (with their index in context.pipes, for removal) that involve `worker`
    readonly property var rows: {
        var rev = context ? context.revision : 0
        if (rev < 0) return []
        var out = []
        var ps = context ? context.pipes : []
        for (var i = 0; i < ps.length; i++)
            if (ps[i].from === worker || ps[i].to === worker)
                out.push({ idx: i, p: ps[i] })
        return out
    }

    // labels for each canonical directive kind (ConfigContext.pipeKinds owns the ordering)
    readonly property var kinds: {
        var labels = ({ pipe: "plain", wrap: "wrap", unwrap: "unwrap", on: "on" })
        var ks = context ? context.pipeKinds : ["pipe", "wrap", "unwrap", "on"]
        var out = []
        for (var i = 0; i < ks.length; i++) out.push({ kind: ks[i], label: labels[ks[i]] })
        return out
    }

    Label {
        text: "Connections (" + panel.rows.length + ")"
        font.bold: true
    }

    // Each pipe is editable in place: change its directive kind and key. A change commits
    // through context.setPipeDirective (which bumps and rebuilds these rows); switching to a
    // keyed kind with no key yet doesn't commit, so the row keeps the user's choice until a
    // key is typed (no fighting the rebuild).
    Repeater {
        model: panel.rows
        delegate: RowLayout {
            Layout.fillWidth: true
            spacing: 4
            property var p: modelData.p
            property int idx: modelData.idx
            property bool outgoing: p.from === panel.worker

            Label {
                text: (outgoing ? "→ " : "← ") + (outgoing ? p.to : p.from)
                Layout.fillWidth: true
                elide: Text.ElideRight
                font.pixelSize: 12
            }
            ComboBox {
                id: kindBox
                Layout.preferredWidth: 86
                model: panel.kinds
                textRole: "label"
                Component.onCompleted: currentIndex = panel.context.dirIndex(panel.context.pipeDir(p))
                onActivated: {
                    var d = panel.kinds[currentIndex]
                    // plain commits immediately; keyed kinds wait for a key in keyField
                    if (d.kind === "pipe") {
                        panel.context.setPipeDirective(idx, "pipe", "")
                        radapter.note("pipe:changed|" + p.from + "|" + p.to + "|pipe")
                    } else if (keyField.text.trim().length > 0) {
                        panel.context.setPipeDirective(idx, d.kind, keyField.text)
                        radapter.note("pipe:changed|" + p.from + "|" + p.to + "|" + d.kind + ":" + keyField.text)
                    }
                }
            }
            TextField {
                id: keyField
                Layout.preferredWidth: 90
                visible: kindBox.currentIndex > 0
                placeholderText: kindBox.currentIndex === 3 ? "field" : "key"
                Component.onCompleted: text = panel.context.pipeKey(p)
                onEditingFinished: {
                    var d = panel.kinds[kindBox.currentIndex]
                    if (d.kind !== "pipe") {
                        panel.context.setPipeDirective(idx, d.kind, text)
                        radapter.note("pipe:changed|" + p.from + "|" + p.to + "|" + d.kind + ":" + text)
                    }
                }
            }
            Button {
                text: "✕"
                implicitWidth: 22
                implicitHeight: 22
                padding: 0
                onClicked: {
                    radapter.note("pipe:removed|" + p.from + "|" + p.to)
                    panel.context.removePipe(idx)
                }
            }
        }
    }

    Label {
        visible: panel.rows.length === 0
        text: "No pipes yet — use Connect on the canvas"
        color: "#999"
        font.pixelSize: 12
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
}
