import QtQuick 2.7
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3
import QtQuick.Window 2.7
import "."

ApplicationWindow {
    id: root
    visible: true
    title: qsTr("Demo: Gauge Controls + Redis + Serial")
    color: "white";

    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    property alias angle: gauge.angle
    property bool kiosk: false


    ColumnLayout {
        anchors.centerIn: parent
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
}
