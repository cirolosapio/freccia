// Runs the bar widget's pure logic (plugin/Model.js) in Node.
import { readFileSync } from "node:fs"
import assert from "node:assert/strict"

const source = readFileSync(new URL("../plugin/Model.js", import.meta.url), "utf8").replace(".pragma library", "")
const module = { exports: {} }
new Function("module", source)(module)
const { parseState, plain, tooltip } = module.exports

const off = { on: false, state: "off", show: "", title: "", error: "" }
assert.deepEqual(parseState(""), off)
assert.deepEqual(parseState("not json"), off)
assert.equal(parseState('{"on":true,"state":"playing","title":"Song"}').state, "playing")
assert.equal(parseState('{"on":true,"state":"exploding"}').state, "off")
assert.equal(parseState('{"on":"yes","state":"playing"}').on, false)
assert.equal(parseState("x".repeat(9000)).state, "off")

// Markup-shaped titles stay literal text for the plain-text tooltip.
const markup = '<img src="https://example.invalid/tracker">'
assert.equal(plain(markup), markup)
assert.equal(plain("a\nb\u0007c"), "a b c")
assert.equal(plain("x".repeat(500)).length, 160)
assert.equal(plain(42), "")

// String.arg as QML has it, for the tooltip.
String.prototype.arg = function (value) { return this.replace("%1", value) }
const t = key => ({ "tooltip.playing": "on", "tooltip.off": "off", "tooltip.retrying": "retry: %1", "tooltip.hint": "hint" })[key] || key
const playing = parseState('{"on":true,"state":"playing","show":"Terra di nessuno","title":"Ligabue – Certe notti"}')
assert.equal(tooltip(playing, t), "on\nTerra di nessuno\n♪ Ligabue – Certe notti\nhint")
assert.equal(tooltip(parseState(""), t), "off\nhint")
assert.equal(tooltip(parseState('{"on":true,"state":"retrying","error":"down","show":"x"}'), t), "retry: down\nhint")

console.log("plugin model: ok")
