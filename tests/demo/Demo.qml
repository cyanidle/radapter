import QtQuick 2.7
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3
import QtQuick.Window 2.7
import QtQuick.Dialogs 1.1
import "."

ApplicationWindow {
    id: root
    visible: true
    title: qsTr("Demo: Gauge Controls + Redis + Serial")
    color: "white";

    width: 400
    height: 400

    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    property alias angle: gauge.angle
    property bool kiosk: false

    MessageDialog {
        id: broke
        title: "SKILL ISSUE"
        text: "YOU ARE BROKE!!!"
        onAccepted: {
            radapter.shutdown()
        }
    }

    Gauge {
        id: gauge
        anchors.centerIn: parent
        size: root.height < root.width ? root.height : root.width
        onAngleChanged: {
            spinBox.value = angle
        }
        SpinBox {
            anchors.centerIn: parent
            id: spinBox
            from: 0
            to: 360
            stepSize: 1
            onValueModified: gauge.angle = value
        }
    }

    Button {
        visible: !root.kiosk
        anchors.top: parent.top
        anchors.left: parent.left
        text: "Toggle Fullscreen"
        onClicked: {
            root.visibility = root.visibility == Window.FullScreen
                ? Window.Windowed
                : Window.FullScreen;
        }
    }

    Button {
        visible: !root.kiosk
        anchors.top: parent.top
        anchors.right: parent.right
        text: "Quit"
        onClicked: {
            radapter.shutdown()
        }
    }
    
    Text {
        id: money
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        property int amount: 1000000
        property int in_flight: 0
        text: in_flight ? `Wait...(${in_flight})` : amount + "$"
    }

    Button {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        text: "GAMBLE!"
        onClicked: {
            if (money.amount <= 0) {
                broke.open()
                return;
            }

            var amount = money.amount * gauge.angle / 360 + 10
            money.in_flight++
            radapter.sendMsg({gamble_request: {amount}});
        }

        function onMsg(msg) {
            var res = msg.gamble_result
            if (res) {
                money.in_flight--
                if (res.ok === true) {
                    console.log(`Amount: ${res.amount}`)
                    money.amount += res.amount
                }
            }
        }

        Component.onCompleted: {
            radapter.msg.connect(onMsg);
        }
    }
}
