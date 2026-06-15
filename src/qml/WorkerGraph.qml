import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

// Canvas view of the objects in a ConfigContext, grouped by type. Each node shows its
// name, type and a badge with how many pipes touch it. Clicking a node selects it
// (nodeClicked) so a host can open its editor; the ✕ removes it. To connect two
// workers, drag the blue nub on a node's right edge onto another worker — that opens
// a popup to pick the pipe directive and adds the pipe (counts update live).
//
// `connectableTypes` lists the types that are pipe endpoints (workers). Objects of any
// other type (e.g. Modbus devices, which are referenced, not piped) are shown as
// non-connectable: no nub, no badge, and they reject drops. Empty = everything connects.
Item {
    id: graph

    property var context: null
    property string selected: ""          // highlighted node (host-driven)
    property var connectableTypes: []     // worker types; empty = all objects connect

    signal nodeClicked(string name)       // a node was selected
    signal nodeRemoved(string name)        // a node was deleted from the canvas

    function isConnectable(name) {
        if (connectableTypes.length === 0) return true
        return connectableTypes.indexOf(context.get(name).type) >= 0
    }
    function beginPipe(from, to) { dirPopup.begin(from, to) }

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
        graph.selected = name
        nodeClicked(name)
    }
    function removeNode(name) {
        context.remove(name)
        if (selected === name) selected = ""
        nodeRemoved(name)
    }

    Rectangle {
        anchors.fill: parent
        color: "#f4f4f4"
        border.color: "#ccc"
        border.width: 1
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
            property bool connectable: graph.isConnectable(nodeName)
            // recompute when pipes change (revision used in a branch so it stays a dep)
            property int conns: (graph.context && graph.context.revision >= 0)
                                ? graph.context.connectionCount(nodeName) : 0

            width: 168
            height: 58
            radius: 6
            color: card.connectable ? "#ffffff" : "#f0f0f5"
            border.width: nodeName === graph.selected ? 2 : 1
            border.color: (dropArea.containsDrag && card.connectable) ? "#fb8c00"
                          : (nodeName === graph.selected ? "#1e88e5" : "#ccc")

            // accept a nub dragged from another worker -> a pipe into this node
            DropArea {
                id: dropArea
                anchors.fill: parent
                keys: ["rad-pipe"]
                onDropped: {
                    var src = drop.source ? drop.source.sourceName : ""
                    if (src && src !== card.nodeName && card.connectable)
                        graph.beginPipe(src, card.nodeName)
                }
            }

            // below the content, so clicks on labels select the node while the ✕
            // button and the drag nub (painted on top) still get their own events
            MouseArea {
                anchors.fill: parent
                onClicked: graph.nodeActivated(card.nodeName)
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                anchors.rightMargin: card.connectable ? 20 : 8   // leave room for the nub
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
                        text: graph.context.get(card.nodeName).type + (card.connectable ? "" : "  · device")
                        font.pixelSize: 10
                        color: "#888"
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                // connection-count badge (workers only; devices aren't pipe endpoints)
                Rectangle {
                    visible: card.connectable
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

            // drag this onto another worker to create a pipe (reparents to the graph
            // while dragging so it floats over other cards; snaps back on release)
            Rectangle {
                id: nub
                visible: card.connectable
                width: 14; height: 14; radius: 7
                color: dragMA.drag.active ? "#fb8c00" : "#1e88e5"
                border.color: "white"; border.width: 2
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 3

                property string sourceName: card.nodeName
                Drag.active: dragMA.drag.active
                Drag.keys: ["rad-pipe"]
                Drag.hotSpot.x: width / 2
                Drag.hotSpot.y: height / 2

                states: State {
                    when: dragMA.drag.active
                    ParentChange { target: nub; parent: graph }
                    AnchorChanges {
                        target: nub
                        anchors.verticalCenter: undefined
                        anchors.right: undefined
                    }
                }

                MouseArea {
                    id: dragMA
                    anchors.fill: parent
                    cursorShape: Qt.CrossCursor
                    drag.target: nub
                    onReleased: nub.Drag.drop()
                }
            }
        }
    }

    // picks the directive for a pipe being drawn (from -> to). The kinds map onto
    // declare's pipe fields: plain / wrap(key) / unwrap(key) / on(field).
    Popup {
        id: dirPopup
        modal: true
        focus: true
        padding: 12
        width: 320
        x: (graph.width - width) / 2
        y: 40
        closePolicy: Popup.CloseOnEscape

        property string fromName: ""
        property string toName: ""

        function begin(from, to) {
            fromName = from; toName = to
            keyField.text = ""; kindBox.currentIndex = 0
            open()
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: "Pipe  " + dirPopup.fromName + "  →  " + dirPopup.toName
                font.bold: true
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: "Directive"; Layout.preferredWidth: 70 }
                ComboBox {
                    id: kindBox
                    Layout.fillWidth: true
                    textRole: "label"
                    model: [
                        { kind: "pipe",   label: "plain pipe" },
                        { kind: "wrap",   label: "wrap(key)" },
                        { kind: "unwrap", label: "unwrap(key)" },
                        { kind: "on",     label: "on(field)" }
                    ]
                }
            }
            RowLayout {
                Layout.fillWidth: true
                visible: kindBox.currentIndex > 0
                Label {
                    text: kindBox.currentIndex === 3 ? "Field" : "Key"
                    Layout.preferredWidth: 70
                }
                TextField {
                    id: keyField
                    Layout.fillWidth: true
                    placeholderText: kindBox.currentIndex === 3 ? "field" : "a:b:[2]"
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button { text: "Cancel"; onClicked: dirPopup.close() }
                Button {
                    text: "Connect"
                    enabled: kindBox.currentIndex === 0 || keyField.text.trim().length > 0
                    onClicked: {
                        var d = kindBox.model[kindBox.currentIndex]
                        graph.context.addPipe(dirPopup.fromName, dirPopup.toName,
                                              { kind: d.kind, key: keyField.text })
                        dirPopup.close()
                    }
                }
            }
        }
    }
}
