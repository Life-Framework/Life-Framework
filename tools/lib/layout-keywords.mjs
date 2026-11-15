// tools/lib/layout-keywords.mjs
//
// Reads the property keywords a .layout file writes directly on each widget, per widget class.
// Shared by tools/gen-layout-keywords.mjs (vanilla, from the game paks) and
// tools/validation/validate-layout-keywords.mjs (this repo's layouts against that list).
//
// A layout is nested blocks. A widget block opens with `<Name>WidgetClass "{GUID}" {`; its direct
// lines are `Key value` or `"Key With Spaces" value`, or open a nested object (`Slot FrameWidgetSlot
// "{..}" {`, `components {`, `FontProperties FontProperties "{..}" {`). The bare `{` opens the
// children. Only keys written directly on a widget are collected: those are what the engine rejects
// with "Unknown keyword/data '<key>' at offset N", and the widget is then dropped from the layout.

const WIDGET = /^([A-Za-z0-9_]+WidgetClass)\b/;

// A key is an identifier or quoted words, followed by a value or a brace. Anything else (the second
// line of a long Text value) is not a property line.
function keyOf(line) {
  const q = /^"([A-Za-z][A-Za-z0-9 _]*)"\s+\S/.exec(line);
  if (q) return q[1];
  const b = /^([A-Za-z_][A-Za-z0-9_]*)\s+\S/.exec(line);
  return b ? b[1] : null;
}

// Returns [{ cls, key, line }] for every key written directly on a widget block.
export function widgetKeys(text) {
  const out = [];
  const stack = [];
  // A text value can span lines; blank its newlines so no continuation line reads as a key.
  const flat = text.replace(/"((?:[^"\\]|\\.)*)"/gs, (m) => m.replace(/\r?\n/g, " "));
  const lines = flat.split("\n");
  for (let i = 0; i < lines.length; i++) {
    const t = lines[i].trim();
    if (!t || t.startsWith("//")) continue;
    const top = stack[stack.length - 1];
    if (t === "}") {
      stack.pop();
      continue;
    }
    if (t.endsWith("{")) {
      const w = WIDGET.exec(t);
      if (w) {
        stack.push({ widget: w[1] });
        continue;
      }
      // A nested object on a widget (Slot, components, FontProperties): its key is a widget key too.
      if (top && top.widget && t !== "{") {
        const k = keyOf(t);
        if (k) out.push({ cls: top.widget, key: k, line: i + 1 });
      }
      stack.push({ widget: null });
      continue;
    }
    if (top && top.widget) {
      const k = keyOf(t);
      if (k) out.push({ cls: top.widget, key: k, line: i + 1 });
    }
  }
  return out;
}
