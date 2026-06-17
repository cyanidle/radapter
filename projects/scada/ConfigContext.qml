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
    // every worker/device schema (type -> schema), for completeness validation
    property var schemas: ({})
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

    // replace the whole authored set from a declare-style config ({ objects, pipes }),
    // e.g. to seed the editor or load an existing config for editing
    function load(config) {
        var newObjects = ({})
        if (config.objects !== undefined)
            for (var name in config.objects) {
                var e = config.objects[name]
                newObjects[name] = { type: e.type, config: e.config !== undefined ? e.config : ({}) }
            }
        var newPipes = []
        if (config.pipes !== undefined)
            for (var i = 0; i < config.pipes.length; i++) newPipes.push(config.pipes[i])
        objects = newObjects
        pipes = newPipes
        bump()
    }
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

    // ---- completeness validation -------------------------------------------
    // a required field is a top-level leaf with no [optional]/[has_default]
    // annotation (mirrors SchemaForm.isRequired); name/category aren't authored
    // here (the object key is the name), so they're excluded
    function requiredKeys(type) {
        var s = schemas[type], out = []
        if (s === undefined) return out
        for (var k in s) {
            if (k === "name" || k === "category") continue
            var fs = s[k]
            if (typeof fs === "string" && fs.indexOf("[") < 0 && fs.split(" ")[0] !== "function")
                out.push(k)
        }
        return out
    }
    // names of required fields left unset on `name` (empty = the object is complete)
    function missingRequired(name) {
        var o = objects[name]
        if (o === undefined) return []
        var req = requiredKeys(o.type), miss = []
        for (var i = 0; i < req.length; i++) {
            var v = o.config[req[i]]
            if (v === undefined || v === "") miss.push(req[i])
        }
        return miss
    }
    function isComplete(name) { return missingRequired(name).length === 0 }
    function allComplete() {
        for (var k in objects) if (!isComplete(k)) return false
        return true
    }

    // ---- pipes (graph edges) ----------------------------------------------
    // a pipe is { from, to } plus at most one declare directive field:
    // wrap=<key> / unwrap=<key> / on=<field> (absent = a plain pipe)
    function pipeDir(p) {
        if (p.wrap !== undefined)   return "wrap"
        if (p.unwrap !== undefined) return "unwrap"
        if (p.on !== undefined)     return "on"
        return ""
    }
    function pipeKey(p) { var f = pipeDir(p); return f === "" ? "" : p[f] }
    function samePipe(a, b) {
        return a.from === b.from && a.to === b.to
            && pipeDir(a) === pipeDir(b) && pipeKey(a) === pipeKey(b)
    }
    function hasPipe(p) {
        for (var i = 0; i < pipes.length; i++)
            if (samePipe(pipes[i], p)) return true
        return false
    }
    // directive: undefined / { kind: "pipe" } -> plain; { kind, key } for wrap|unwrap|on
    function addPipe(from, to, directive) {
        if (from === to) return false
        var p = { from: from, to: to }
        if (directive !== undefined && directive.kind !== undefined && directive.kind !== "pipe") {
            var key = directive.key === undefined ? "" : String(directive.key).trim()
            if (key.length === 0) return false   // wrap/unwrap/on require a key
            p[directive.kind] = key
        }
        if (hasPipe(p)) return false
        pipes = pipes.concat([p])
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
