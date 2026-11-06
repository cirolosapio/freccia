.pragma library

// Pure helpers for Widget.qml, kept apart so tests can run them in Node.

// The state file is written by the Freccia app; anything unexpected reads as off.
function parseState(text) {
  var off = { on: false, state: "off", show: "", title: "", error: "" }
  if (!text || text.length > 8192) return off
  var data
  try { data = JSON.parse(text) } catch (e) { return off }
  if (!data || typeof data !== "object") return off
  var states = ["off", "connecting", "playing", "retrying"]
  return {
    on: data.on === true,
    state: states.indexOf(data.state) >= 0 ? data.state : "off",
    show: plain(data.show),
    title: plain(data.title),
    error: plain(data.error)
  }
}

// Song titles and error text come from the network. The bar's shared
// tooltip draws plain text, so markup stays literal; this only keeps the
// value to one short line.
function plain(value) {
  if (typeof value !== "string") return ""
  return value.replace(/[\u0000-\u001f\u007f]/g, " ").slice(0, 160)
}

// The tooltip: what the radio is doing, the show and song when known, then
// how to use the button. t looks up the widget's translated strings.
function tooltip(radio, t) {
  var lines = []
  if (radio.state === "playing") lines.push(t("tooltip.playing"))
  else if (radio.state === "connecting") lines.push(t("tooltip.connecting"))
  else if (radio.state === "retrying") lines.push(t("tooltip.retrying").arg(radio.error))
  else lines.push(t("tooltip.off"))
  if (radio.on && radio.state !== "retrying") {
    if (radio.show) lines.push(radio.show)
    if (radio.title) lines.push("♪ " + radio.title)
  }
  lines.push(t("tooltip.hint"))
  return lines.join("\n")
}

if (typeof module !== "undefined") module.exports = { parseState: parseState, plain: plain, tooltip: tooltip }
