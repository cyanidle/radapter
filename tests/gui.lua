local component = temp_file [[

import QtQuick 2.3

Rectangle { 
    width: 300; 
    height: 300; 
    color: "red" 
}

]]

local view = Qml {
    url = component:url()
}