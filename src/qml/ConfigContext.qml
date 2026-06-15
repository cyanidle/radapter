import QtQuick 2.7

// Shared authoring state for one or more WorkerConfigurators. Holds every object
// (worker or device) being configured, keyed by name, so several editors see a single
// consistent set: names stay unique across editors, and a device created in one editor's
// ref field is selectable from another. This is the seed of the future multi-object
// graph: each node is one entry here.
QtObject {
    id: ctx

    // name -> { type, config }; config is the live values object the editor mutates
    property var objects: ({})
    // pipes between objects, as [ { from, to } ] (the graph editor's edges)
    property var pipes: []
    // QML can't observe deep mutation of a var map, so observers bind to `revision`
    // (bumped on every structural change) to re-evaluate candidate lists / validation
    property int revision: 0
    signal changed()

    function bump()        { revision += 1; changed() }
    function has(name)     { return objects[name] !== undefined }
    function get(name)     { return objects[name] }

    // is `name` usable by an editor that currently occupies `owned` ("" if none)?
    function nameAvailable(name, owned) {
        return name.length > 0 && (!has(name) || name === owned)
    }

    function put(name, type, config) { objects[name] = { type: type, config: config }; bump() }
    function remove(name) {
        if (!has(name)) return
        delete objects[name]
        pipes = pipes.filter(function (p) { return p.from !== name && p.to !== name })
        bump()
    }
    // keep edges intact when an editor renames its object
    function rename(oldName, newName) {
        if (!has(oldName) || oldName === newName) return
        objects[newName] = objects[oldName]
        delete objects[oldName]
        pipes = pipes.map(function (p) {
            return { from: p.from === oldName ? newName : p.from,
                     to:   p.to   === oldName ? newName : p.to }
        })
        bump()
    }

    // names whose object type is in `types` (for ref-candidate lists)
    function ofTypes(types) {
        var out = []
        for (var k in objects) if (types.indexOf(objects[k].type) >= 0) out.push(k)
        return out
    }

    // ---- pipes (graph edges) ----------------------------------------------
    function hasPipe(from, to) {
        for (var i = 0; i < pipes.length; i++)
            if (pipes[i].from === from && pipes[i].to === to) return true
        return false
    }
    function addPipe(from, to) {
        if (from === to || hasPipe(from, to)) return false
        pipes = pipes.concat([{ from: from, to: to }])
        bump()
        return true
    }
    function removePipe(i) {
        pipes = pipes.filter(function (_, j) { return j !== i })
        bump()
    }
    // how many pipes touch `name` (in or out) — shown on each graph node
    function connectionCount(name) {
        var c = 0
        for (var i = 0; i < pipes.length; i++)
            if (pipes[i].from === name || pipes[i].to === name) c++
        return c
    }
}
