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

    function describe(p) {
        var outgoing = p.from === worker
        var arrow = outgoing ? "→" : "←"
        var other = outgoing ? p.to : p.from
        var dir = context.pipeDir(p)
        var extra = dir === "" ? "" : "  [" + dir + ":" + context.pipeKey(p) + "]"
        return arrow + "  " + other + extra
    }

    Label {
        text: "Connections (" + panel.rows.length + ")"
        font.bold: true
    }

    Repeater {
        model: panel.rows
        delegate: RowLayout {
            Layout.fillWidth: true
            spacing: 4
            Label {
                text: panel.describe(modelData.p)
                Layout.fillWidth: true
                elide: Text.ElideRight
                font.pixelSize: 12
            }
            Button {
                text: "✕"
                implicitWidth: 22
                implicitHeight: 22
                padding: 0
                onClicked: panel.context.removePipe(modelData.idx)
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
