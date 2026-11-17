// tools/scout/dump.mjs - reads the entity dump a Workbench `look --dump` writes (dump.txt beside the shots).
//
// The rows come from the `dump` directive of tools/visual/wb/LF_VisualShotsPlugin.c (DumpArea), which asks the
// engine itself for every entity near a place: vanilla Everon's world file is a binary the tools do not decode,
// and the editor is the one reader that cannot be wrong about it. One row per line, space separated:
//
//   H <name> <x> <z> <radius> <step>                    the request
//   E <i> <class> <prefab|-> <x> <y> <z> <yaw> <scale> <minx> <miny> <minz> <maxx> <maxy> <maxz> <name|->
//   D <i> <prefab|-> <x> <y> <z>                        a door entity below entity i (an entrance)
//   T <x> <z>                                           a tree or bush
//   R <class> <name|-> <n> x y z x y z ...              a road or other spline, world space
//   RV <class> <var> <var> ...                          a spline whose points the plugin could not read: its variable names
//   C <class> <count>                                   entities of each class the query returned
//   G <x0> <z0> <step> <nx> <nz>                        ground grid header; GH <j> y y y ...  heights of row j (z), GW <j> 0101...  1 = under the sea
//
// Yaw is degrees, Enfusion's convention: 0 faces +z (north), 90 faces +x (east).

const num = (s) => Number(s);

// Pure: dump text -> { request, entities, trees, roads, roadVars, classes, grid }.
export function parseDump(text) {
  const out = { request: null, entities: [], trees: [], roads: [], roadVars: {}, classes: {}, grid: null };
  const byIndex = new Map();
  let grid = null;
  for (const raw of String(text).split(/\r?\n/)) {
    const line = raw.trim();
    if (!line) continue;
    const t = line.split(/\s+/);
    switch (t[0]) {
      case "H":
        out.request = { name: t[1], x: num(t[2]), z: num(t[3]), radius: num(t[4]), step: num(t[5]) };
        break;
      case "E": {
        const e = {
          i: num(t[1]),
          cls: t[2],
          prefab: t[3] === "-" ? "" : t[3],
          x: num(t[4]),
          y: num(t[5]),
          z: num(t[6]),
          yaw: num(t[7]),
          scale: num(t[8]),
          min: [num(t[9]), num(t[10]), num(t[11])],
          max: [num(t[12]), num(t[13]), num(t[14])],
          name: t.slice(15).join(" ") === "-" ? "" : t.slice(15).join(" "),
          doors: [],
        };
        out.entities.push(e);
        byIndex.set(e.i, e);
        break;
      }
      case "D": {
        const owner = byIndex.get(num(t[1]));
        if (owner) owner.doors.push({ prefab: t[2] === "-" ? "" : t[2], x: num(t[3]), y: num(t[4]), z: num(t[5]) });
        break;
      }
      case "T":
        out.trees.push([num(t[1]), num(t[2])]);
        break;
      case "R": {
        const n = num(t[3]);
        const points = [];
        for (let k = 0; k < n; k++) points.push([num(t[4 + k * 3]), num(t[5 + k * 3]), num(t[6 + k * 3])]);
        out.roads.push({ cls: t[1], name: t[2] === "-" ? "" : t[2], points });
        break;
      }
      case "RV":
        out.roadVars[t[1]] = t.slice(2);
        break;
      case "C":
        out.classes[t[1]] = num(t[2]);
        break;
      case "G":
        grid = { x0: num(t[1]), z0: num(t[2]), step: num(t[3]), nx: num(t[4]), nz: num(t[5]), h: [], water: [] };
        out.grid = grid;
        break;
      case "GH":
        if (grid) grid.h[num(t[1])] = t.slice(2).map(num);
        break;
      case "GW":
        if (grid) grid.water[num(t[1])] = [...(t[2] ?? "")].map((c) => c === "1");
        break;
      default:
        break;
    }
  }
  return out;
}

// Pure: one dump as the text the plugin writes (the selftest's fixtures are made with it).
export function formatDump({ request, entities = [], trees = [], roads = [], classes = {}, grid = null }) {
  const rows = [];
  if (request) rows.push(`H ${request.name} ${request.x} ${request.z} ${request.radius} ${request.step}`);
  entities.forEach((e, i) => {
    rows.push(`E ${i} ${e.cls ?? "GenericEntity"} ${e.prefab || "-"} ${e.x} ${e.y} ${e.z} ${e.yaw ?? 0} ${e.scale ?? 1} ${e.min.join(" ")} ${e.max.join(" ")} ${e.name || "-"}`);
    for (const d of e.doors ?? []) rows.push(`D ${i} ${d.prefab || "-"} ${d.x} ${d.y} ${d.z}`);
  });
  for (const [x, z] of trees) rows.push(`T ${x} ${z}`);
  for (const r of roads) rows.push(`R ${r.cls ?? "RoadEntity"} ${r.name || "-"} ${r.points.length} ${r.points.map((p) => p.join(" ")).join(" ")}`);
  for (const [c, n] of Object.entries(classes)) rows.push(`C ${c} ${n}`);
  if (grid) {
    rows.push(`G ${grid.x0} ${grid.z0} ${grid.step} ${grid.nx} ${grid.nz}`);
    grid.h.forEach((r, j) => {
      rows.push(`GH ${j} ${r.join(" ")}`);
      rows.push(`GW ${j} ${(grid.water?.[j] ?? r.map(() => false)).map((w) => (w ? "1" : "0")).join("")}`);
    });
  }
  return rows.join("\n") + "\n";
}
