.pragma library

// Shared helpers for the HMI node tree (used by HmiEditor.qml and Node.qml). A node is a
// layout CONTAINER (Row/Column/Grid, with `children`) or a widget LEAF; a path is an array
// of child indices addressing one node from the root.

function isContainer(type) {
    return type === "Row" || type === "Column" || type === "Grid"
}

// structural equality of two index paths (null is never equal to anything)
function pathEq(a, b) {
    if (a === null || b === null) return false
    if (a.length !== b.length) return false
    for (var i = 0; i < a.length; i++) if (a[i] !== b[i]) return false
    return true
}
