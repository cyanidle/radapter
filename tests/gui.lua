local window = gui.QMainWindow()

local view = gui.QQuickView(window) --TODO: pass arguments to ctor, to embed view into window

view:setSource("./test.qml")

view:show()
window:show()
