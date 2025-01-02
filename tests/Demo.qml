import QtQuick 2.7
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3
import "."

ApplicationWindow {
    visible: true
    width: 640
    height: 480
    title: qsTr("Demo: Gauge Controls from Redis")
    color: "white";

    property alias angle: gauge.angle

    // onAngleChanged: {
    //     console.log("Angle:", angle)
    // }

    Row {
        Gauge {
            id: gauge
            onAngleChanged: spinBox.value = angle
        }
        SpinBox {
            id: spinBox
            from: 0
            to: 360
            stepSize: 1
            onValueModified: gauge.angle = value
        }
    }
}
