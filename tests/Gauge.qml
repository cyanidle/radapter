import QtQuick 2.7
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3

Item {
    id: myGauge

    property real size: 400;
    property real angle: 0

    // private:

    width: size;
    height: size;

    Rectangle {
        id: background
        anchors.fill: parent
        color: "grey";
        radius: myGauge.size / 2.0;
        border.color: "red";
        border.width: 5;
    }

    Rectangle {
        id: needle

        property real len: myGauge.size / 2.0 * 0.85;
        property real w: 10;

        color: "white";
        width: w;
        height: len
        antialiasing: true;

        transform: [
            Rotation { origin.x: 0; origin.y: 0; angle: myGauge.angle; }
            , Translate { x: myGauge.size / 2.0 - needle.w / 2.0; y: myGauge.size / 2.0 - needle.w / 2.0 }
        ]
    }

    MouseArea {
        anchors.fill: parent
        property bool isDragMode: false
        hoverEnabled: true;

        function setAngleFromPosition(px, py) {
            var y1 = myGauge.size / 2.0;
            var x1 = myGauge.size / 2.0;
            var y2 = py;
            var x2 = px;

            var m = (y2 - y1) / (x2 - x1);
            var alpha = Math.atan(m) * (180.0 / Math.PI);;

            if (x2 < x1) {
                myGauge.angle = alpha + 90;
            } else {
                myGauge.angle = alpha + 270;
            }
        }

        onReleased: isDragMode = false;

        onPressAndHold: {
            isDragMode = true;
            setAngleFromPosition(mouse.x, mouse.y)
        }

        onPositionChanged: {
            if (!isDragMode)
                return;
            setAngleFromPosition(mouse.x, mouse.y)
        }
    }

}
