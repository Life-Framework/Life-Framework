// tools/lib/quoted-vectors.mjs - find a vector written as a quoted string (validate-quoted-vectors).
//
// Enfusion text resources write a vector bare: `m_vHalfExtents 5 3 7`, `coords 1 2 3`. A quoted
// `m_vHalfExtents "5 3 7"` is a string where the engine wants a vector: the load logs
// `WORLD (E): Unexpected data at offset` and drops the entity at world origin (the Saint-Philippe
// weighbridge prompt, found by the harvester proof, #3665; no player could unload grain).
//
// Pure helpers; the validator owns the file walk.

// Vector property names. `m_v` plus a capital is the Enfusion member convention for a vector;
// the others are the entity and PointInfo properties a layer or prefab writes as a vector.
const VEC_NAME = String.raw`(?:m_v[A-Z][A-Za-z0-9_]*|coords|angles|Offset)`;
const VEC_STRICT_NAME = String.raw`m_v[A-Z][A-Za-z0-9_]*`;

// `name "n n n"` on its own line in a resource file: two or more numbers inside the quotes.
const NUM = String.raw`-?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?`;
const quotedResource = (name) => new RegExp(String.raw`^\s*(${name})\s+"\s*${NUM}(?:\s+${NUM})+\s*"\s*$`);
const RESOURCE_ANY = quotedResource(VEC_NAME);
const RESOURCE_STRICT = quotedResource(VEC_STRICT_NAME);

// A generator writing the same thing: `m_vHalfExtents "5 3 7"` or `coords "${x} ${y} ${z}"` inside a
// JS string (quotes escaped or in a template), so the generated file is wrong however it is read.
const GENERATOR = new RegExp(String.raw`(?:^|[^A-Za-z0-9_])(${VEC_NAME})\s+\\?"(?:-?\d|\$\{)[^"\n]* [^"\n]*\\?"`);

/**
 * Lines of a .layer/.et/.conf text with a known vector property holding a quoted number list.
 * `strict` limits the names to the `m_v*` members: a `.conf` may legitimately keep a string such
 * as `Pos "4592.3 0 10818.2"` that a script parses, so only a `m_v*` member is certain there.
 * Returns [{ line, name, text }], line 1-based.
 */
export function quotedVectorLines(text, { strict = false } = {}) {
  const re = strict ? RESOURCE_STRICT : RESOURCE_ANY;
  const out = [];
  const lines = String(text).split("\n");
  for (let i = 0; i < lines.length; i++) {
    const m = re.exec(lines[i].replace(/\r$/, ""));
    if (m) out.push({ line: i + 1, name: m[1], text: lines[i].trim() });
  }
  return out;
}

/** Lines of generator (JS) source that write a quoted vector into the resource they emit. */
export function quotedVectorEmitters(text) {
  const out = [];
  const lines = String(text).split("\n");
  for (let i = 0; i < lines.length; i++) {
    if (/^\s*(\/\/|\*)/.test(lines[i]) || lines[i].includes("lf-quoted-vector-ok")) continue;
    const m = GENERATOR.exec(lines[i]);
    if (m) out.push({ line: i + 1, name: m[1], text: lines[i].trim().slice(0, 160) });
  }
  return out;
}
