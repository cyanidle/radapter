import QtQuick 2.7
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3
import "."

ApplicationWindow {
    visible: true
    property int size: 400
    width: size
    height: width
    title: qsTr("Demo: Gauge Controls from Redis")
    color: "white";

    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    property alias angle: gauge.angle

    ColumnLayout {
        Gauge {
            id: gauge
            onAngleChanged: spinBox.value = angle
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
}
