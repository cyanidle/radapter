import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

ApplicationWindow {
    id: root
    visible: true
    width: 420
    height: 520

    property string myName: ""
    property string status: "connecting…"

    title: myName.length ? "Chat — " + myName + (status.length ? " (" + status + ")" : "")
                         : "Radapter Chat"

    function addMessage(from, text, mine) {
        chatModel.append({ "who": from, "body": String(text), "mine": mine === true })
    }

    function onMsg(msg) {
        if (msg.text !== undefined)
            addMessage(msg.from || "?", msg.text, false)
        if (msg.status !== undefined)
            root.status = String(msg.status)
    }

    Component.onCompleted: radapter.model.received.connect(onMsg)

    // ── Nickname overlay ─────────────────────────────────────────────────────
    Rectangle {
        id: joinOverlay
        anchors.fill: parent
        visible: !root.myName.length
        color: "#1e2127"
        z: 10

        function doJoin() {
            var n = nickField.text
            if (n.length > 0) root.myName = n
        }

        Column {
            anchors.centerIn: parent
            spacing: 14

            Text {
                text: "Choose a nickname"
                color: "white"
                font.pixelSize: 16
                anchors.horizontalCenter: parent.horizontalCenter
            }

            TextField {
                id: nickField
                width: 220
                placeholderText: "nickname…"
                onAccepted: joinOverlay.doJoin()
            }

            Button {
                text: "Join"
                anchors.horizontalCenter: parent.horizontalCenter
                onClicked: joinOverlay.doJoin()
            }
        }
    }

    // ── Chat ─────────────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        ListView {
            id: chatView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: ListModel { id: chatModel }
            onCountChanged: positionViewAtEnd()

            delegate: Item {
                width: chatView.width
                height: bubble.height + 4
                Rectangle {
                    id: bubble
                    radius: 8
                    color: model.mine ? "#2d7d46" : "#3a3f4b"
                    width: Math.min(content.implicitWidth + 16, chatView.width * 0.8)
                    height: content.implicitHeight + 12
                    anchors.right: model.mine ? parent.right : undefined
                    anchors.left:  model.mine ? undefined : parent.left
                    Column {
                        id: content
                        x: 8; y: 6
                        width: chatView.width * 0.8 - 16
                        Text {
                            text: model.who
                            color: "#cfd8dc"
                            font.pixelSize: 10
                            font.bold: true
                        }
                        Text {
                            text: model.body
                            color: "white"
                            width: parent.width
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            enabled: root.myName.length > 0

            TextField {
                id: msgInput
                Layout.fillWidth: true
                placeholderText: root.myName.length ? "Type a message…" : "Join first…"
                onAccepted: sendButton.send()
            }

            Button {
                id: sendButton
                text: "Send"
                onClicked: send()
                function send() {
                    var t = msgInput.text
                    if (t.length === 0 || root.myName.length === 0) return
                    root.addMessage(root.myName, t, true)
                    radapter.model.send({ from: root.myName, text: t })
                    msgInput.text = ""
                }
            }
        }
    }
}
