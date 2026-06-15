import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

// Canvas view of the objects in a ConfigContext, grouped by worker type. Each node
// shows its name, its type and a badge with how many pipes touch it. Clicking a node
// selects it (nodeClicked) so a host can open its editor; the ✕ removes it. In
// `connectMode` the first click picks a pipe source and the second creates the pipe
// (context.addPipe), so the connection counts update live.
Item {
    id: graph

    property var context: null
    property string selected: ""          // highlighted node (host-driven)
    property bool connectMode: false      // clicks build pipes instead of selecting
    property string connectFrom: ""       // pending pipe source while connecting

    signal nodeClicked(string name)       // a node was selected (not in connect mode)
    signal nodeRemoved(string name)        // a node was deleted from the canvas

    // re-read the (deeply-mutated) context whenever it bumps its revision; the
    // revision is used in a branch (not a dead local) so the dependency survives
    readonly property var groups: {
        var rev = context ? context.revision : 0
        if (rev < 0) return []
        var objs = context ? context.objects : ({})
        var byType = ({})
        for (var name in objs) {
            var t = objs[name].type
            if (byType[t] === undefined) byType[t] = []
            byType[t].push(name)
        }
        var types = Object.keys(byType).sort()
        var out = []
        for (var i = 0; i < types.length; i++) {
            byType[types[i]].sort()
            out.push({ type: types[i], names: byType[types[i]] })
        }
        return out
    }

    function nodeActivated(name) {
        if (connectMode) {
            if (connectFrom.length === 0) { connectFrom = name; return }
            if (connectFrom !== name) context.addPipe(connectFrom, name)
            connectFrom = ""
            return
        }
        graph.selected = name
        nodeClicked(name)
    }
    function removeNode(name) {
        context.remove(name)
        if (selected === name) selected = ""
        if (connectFrom === name) connectFrom = ""
        nodeRemoved(name)
    }

    Rectangle {
        anchors.fill: parent
        color: "#f4f4f4"
        border.color: graph.connectMode ? "#fb8c00" : "#ccc"
        border.width: graph.connectMode ? 2 : 1
        radius: 4

        Label {
            anchors.centerIn: parent
            visible: graph.groups.length === 0
            text: "No workers yet — add one above"
            color: "#999"
        }

        ScrollView {
            anchors.fill: parent
            anchors.margins: 8
            clip: true

            ColumnLayout {
                width: graph.width - 32
                spacing: 10

                Repeater {
                    model: graph.groups
                    delegate: ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        property var group: modelData

                        Label {
                            text: group.type + "  (" + group.names.length + ")"
                            font.bold: true
                            font.pixelSize: 12
                            color: "#555"
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 8

                            Repeater {
                                model: group.names
                                delegate: nodeCard
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: nodeCard
        Rectangle {
            id: card
            property string nodeName: modelData
            // recompute when pipes change (revision used in a branch so it stays a dep)
            property int conns: (graph.context && graph.context.revision >= 0)
                                ? graph.context.connectionCount(nodeName) : 0

            width: 168
            height: 58
            radius: 6
            color: graph.connectFrom === nodeName ? "#fff3e0" : "#ffffff"
            border.width: nodeName === graph.selected ? 2 : 1
            border.color: nodeName === graph.selected ? "#1e88e5"
                          : (graph.connectFrom === nodeName ? "#fb8c00" : "#ccc")

            // below the content, so clicks on labels select the node while the ✕
            // button (interactive, painted on top) still gets its own clicks
            MouseArea {
                anchors.fill: parent
                onClicked: graph.nodeActivated(card.nodeName)
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Label {
                        text: card.nodeName
                        font.bold: true
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: graph.context.get(card.nodeName).type
                        font.pixelSize: 10
                        color: "#888"
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                // connection-count badge
                Rectangle {
                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: Math.max(20, countLabel.implicitWidth + 8)
                    implicitHeight: 20
                    radius: 10
                    color: card.conns > 0 ? "#1e88e5" : "#cfcfcf"
                    Label {
                        id: countLabel
                        anchors.centerIn: parent
                        text: card.conns
                        color: "white"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                Button {
                    text: "✕"
                    implicitWidth: 24
                    implicitHeight: 24
                    padding: 0
                    onClicked: graph.removeNode(card.nodeName)
                }
            }
        }
    }
}
