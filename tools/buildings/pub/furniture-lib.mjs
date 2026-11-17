// Parse and flatten a base-game furniture prefab (the .et text) into one standalone entity per prop.
// The base game groups props of one prefab under `$grp Class : "ref" { {..} {..} }`; our pack copy writes each
// as its own child so a pruned set stays readable. Pure text work: no game needed once the text is in hand.

// A node: { head, lines: [own property lines], kids: [node] }. An anonymous block has head "{".
export function parse(text) {
  const root = { head: "", lines: [], kids: [] };
  const stack = [root];
  for (const raw of text.split("\n")) {
    const line = raw.replace(/\r$/, "");
    const t = line.trim();
    if (!t || t.startsWith("//")) continue;
    const top = stack[stack.length - 1];
    if (t === "}") {
      stack.pop();
    } else if (t.endsWith("{")) {
      const node = { head: t.replace(/\s*\{$/, "") || "{", lines: [], kids: [] };
      top.kids.push(node);
      stack.push(node);
    } else {
      top.lines.push(t);
    }
  }
  return root;
}

// The entity list of a furniture prefab: the root's anonymous child block, grouped entries expanded.
export function entities(text) {
  const root = parse(text).kids[0];
  const list = root.kids.find((k) => k.head === "{") ?? root;
  const out = [];
  for (const k of list.kids) {
    const g = /^\$grp (\w+) : "([^"]+)"$/.exec(k.head);
    if (g) {
      for (const inner of k.kids) out.push(entity(`${g[1]} : "${g[2]}"`, inner));
    } else {
      out.push(entity(k.head, k));
    }
  }
  return out;
}

function entity(head, node) {
  const m = /^(?:\$grp )?(\w+)(?: (\w+))? : "(\{[0-9A-F]+\})([^"]+)"$/.exec(head);
  const own = (key) => node.lines.find((l) => l.startsWith(`${key} `))?.slice(key.length + 1);
  const xyz = (s) => (s ? s.split(/\s+/).map(Number) : [0, 0, 0]);
  const c = xyz(own("coords"));
  const a = own("angles") ? xyz(own("angles")) : [0, Number(own("angleY") ?? 0), 0];
  return { cls: m?.[1] ?? "GenericEntity", guid: m?.[3], path: m?.[4], name: node.head.split(":")[0].trim(), node, x: c[0], y: c[1], z: c[2], yaw: a[1], angles: a };
}

const emit = (node, depth) => {
  const pad = " ".repeat(depth);
  const out = [`${pad}${node.head === "{" ? "{" : `${node.head} {`}`];
  for (const l of node.lines) out.push(`${pad} ${l}`);
  for (const k of node.kids) out.push(...emit(k, depth + 1));
  out.push(`${pad}}`);
  return out;
};

// The entity as a child of the furniture block (two spaces in).
export function emitEntity(e) {
  const head = `${e.cls} : "${e.guid}${e.path}"`;
  return emit({ head, lines: e.node.lines, kids: e.node.kids }, 2).join("\n");
}

// A floor from the height: the taproom floor is y 0 (its ceiling lamps hang at 3.3), the guest floor about 3.8, the
// attic from 7.0 (the measured roof slab at 7.07).
export function floorOf(y) {
  if (y < 3.6) return 0;
  if (y < 7.0) return 1;
  return 2;
}

export const short = (p) => p.replace(/^Prefabs\/(Props|Structures)\//, "").replace(/\.et$/, "");
