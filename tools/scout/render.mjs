// tools/scout/render.mjs - dossier.json -> dossier.md. Pure: the same JSON always renders the same text.

const cell = (s) => String(s ?? "").replace(/\|/g, "\\|").replace(/\n/g, " ");
const table = (head, rows) => (rows.length ? [`| ${head.join(" | ")} |`, `| ${head.map(() => "---").join(" | ")} |`, ...rows.map((r) => `| ${r.map(cell).join(" | ")} |`)].join("\n") : "_none_");
const img = (p) => (p ? `![${cell(p.caption)}](${p.file})` : "");

// Pure: the markdown of a dossier object (schema in tools/scout/scout.mjs buildDossier).
export function renderDossier(d) {
  const L = [];
  const s = d.summary;
  const place = d.place;
  L.push(`# ${place.name}: location dossier`);
  L.push("");
  L.push(`Generated ${d.generated.at} by \`tools\\cli scout\`. Do not edit by hand; run \`${d.generated.command}\` to refresh. The machine-readable twin is \`dossier.json\`.`);
  L.push("");
  L.push(`- **Place:** ${place.name}${place.kind ? ` (${place.kind})` : ""}, ${place.source === "gazetteer" ? "from the map's own label" : "from coordinates"}`);
  L.push(`- **Centre:** x ${place.x}, z ${place.z}, ground ${place.y ?? "?"} m; **radius** ${d.radius} m`);
  L.push(`- **World:** ${d.generated.world}, ${d.generated.gameBuild}`);
  if (d.nearbyPlaces?.length) L.push(`- **Other labels nearby:** ${d.nearbyPlaces.map((p) => `${p.name} (${p.kind}, ${p.distance} m ${p.direction})`).join("; ")}`);
  L.push("");

  L.push("## At a glance");
  L.push("");
  L.push(`${s.buildings} buildings (${s.homes} homes, ${s.civic} civic, ${s.ruins} ruins), ${s.enterable} enterable (a door entity below them), ${s.entrances} entrances from outside among ${s.doors} door entities; ${s.roads} road(s), ${s.junctions} junction(s); ${s.props} notable props; ${s.trees} trees, bushes and other generated vegetation; ${s.freePlots} free plot(s).`);
  L.push("");
  L.push(table(["Type", "Count"], Object.entries(s.byType).sort((a, b) => b[1] - a[1]).map(([t, n]) => [t, n])));
  L.push("");

  const pics = (role) => d.pictures.filter((p) => p.role === role && p.file);
  if (pics("overview").length) {
    L.push("## Overview");
    L.push("");
    for (const p of pics("overview")) L.push(`${img(p)}\n`);
  }

  L.push("## Terrain");
  L.push("");
  for (const n of d.terrain.notes ?? []) L.push(`- ${n}`);
  L.push("");

  L.push("## Roads and junctions");
  L.push("");
  if (d.mainStreet) {
    L.push(`**Main street:** ${d.mainStreet.road}, ${d.mainStreet.buildingsAlong} buildings within 25 m of it; its busiest stretch is at x ${d.mainStreet.at[0]}, z ${d.mainStreet.at[1]}.`);
    L.push("");
    for (const p of pics("main-street")) L.push(`${img(p)}\n`);
  } else L.push("No main street could be picked (no road geometry in the dump).\n");
  L.push(table(["Id", "Road", "Length in radius (m)", "Points"], d.roads.map((r) => [r.id, r.name, r.length, r.points.length])));
  L.push("");
  L.push(table(["x", "z", "Kind", "Arms", "Roads"], d.junctions.map((j) => [j.x, j.z, j.kind, j.arms, j.roads.join(", ")])));
  L.push("");

  L.push("## Civic candidates");
  L.push("");
  L.push("Public-facing buildings a town plan would use (church, chapel, government, police, fire, pub, shop, fuel station, hotel, recreation), nearest first. Each links its pictures.");
  L.push("");
  const civic = d.buildings.filter((b) => b.civic && !b.ruin);
  L.push(table(["Id", "Type", "Vanilla prefab", "Distance", "Footprint", "Floors", "Interior", "Entrances"], civic.map((b) => [b.id, b.type, b.stem, `${b.distance} m ${b.bearing}`, `${b.footprint.w} x ${b.footprint.d} m (${b.footprint.area} m2)`, b.floors ?? "-", b.interior.value, b.entrances.length ? b.entrances.map((e) => `${e.side}/${e.facing}`).join(", ") : b.entrancesKnown === false ? "?" : "-"])));
  L.push("");
  for (const b of civic) {
    const ps = d.pictures.filter((p) => p.building === b.id && p.file);
    if (!ps.length) continue;
    L.push(`### ${b.id} ${b.type}: ${b.stem}`);
    L.push("");
    L.push(`\`${b.prefab}\` at x ${b.x}, z ${b.z}, facing ${b.facing}; footprint ${b.footprint.w} x ${b.footprint.d} m; interior ${b.interior.value} (${b.interior.basis}).`);
    L.push("");
    for (const p of ps) L.push(`${img(p)}\n`);
  }

  L.push("## All buildings");
  L.push("");
  L.push(table(["Id", "Type", "Vanilla prefab", "x", "z", "Yaw", "Facing", "Footprint (m)", "Floors", "Interior", "Entrances", "Furniture"], d.buildings.map((b) => [b.id, b.type, b.stem, b.x, b.z, b.yaw, b.facing, `${b.footprint.w} x ${b.footprint.d} (${b.footprint.area} m2)`, b.floors ?? "-", b.interior.value, b.entrances.length || (b.entrancesKnown === false ? "?" : "-"), b.furnitureInside ?? "-"])));
  L.push("");
  L.push("Footprint: `catalog` and `measure` are the model's own extents turned by the yaw; `world-bounds` is the engine's axis-aligned box. Yaw is Enfusion's (0 faces +z north, 90 faces +x east). Interior `yes` means door entities and a walkable floor (a heuristic, not walked); `unknown` means door entities but no walkable floor. Entrances are the doors on the outline; `?` means the building was not found in the engine dump. Furniture is the count of furnishing props inside the footprint where the place file has it.");
  L.push("");

  L.push("## Notable props");
  L.push("");
  L.push(table(["Kind", "Vanilla prefab", "x", "z", "Distance"], d.props.filter((p) => p.kind !== "street-lamp" && p.kind !== "manhole").map((p) => [p.kind, p.stem, p.x, p.z, `${p.distance} m`])));
  L.push("");
  const counted = Object.entries(d.propCounts).filter(([k]) => k === "street-lamp" || k === "manhole");
  if (counted.length) L.push(`Counted, not listed: ${counted.map(([k, n]) => `${n} ${k}`).join(", ")}.\n`);

  L.push("## Free plots");
  L.push("");
  L.push(table(["Id", "x", "z", "Size (m)", "Area (m2)", "Ground (m)", "Nearest road", "Nearest building", "From centre"], d.freePlots.plots.map((p) => [p.id, p.x, p.z, `${p.width} x ${p.depth}`, p.area, `${p.ground.min} to ${p.ground.max}`, p.nearestRoad ? `${p.nearestRoad.name} ${p.nearestRoad.distance} m` : "-", p.nearestBuilding ? `${p.nearestBuilding.id} ${p.nearestBuilding.type} ${p.nearestBuilding.distance} m` : "-", `${p.distance} m`])));
  L.push("");
  L.push(d.freePlots.note);
  L.push("");

  const interiors = d.pictures.filter((p) => p.role === "interior" && p.file);
  if (interiors.length) {
    L.push("## Interiors");
    L.push("");
    for (const p of interiors) L.push(`${img(p)}\n`);
  }

  const missing = d.pictures.filter((p) => !p.file);
  if (d.notes.length || missing.length) {
    L.push("## Limits of this dossier");
    L.push("");
    for (const n of d.notes) L.push(`- ${n}`);
    for (const p of missing) L.push(`- picture \`${p.role}${p.building ? ` ${p.building}` : ""}\` missing: ${p.missing}`);
    L.push("");
  }

  L.push("## Prefab references");
  L.push("");
  L.push("Every distinct building prefab, as the world file refers to it.");
  L.push("");
  const seen = new Map();
  for (const b of d.buildings) seen.set(b.prefab, (seen.get(b.prefab) ?? 0) + 1);
  L.push(table(["Prefab", "Type", "Count"], [...seen].map(([p, n]) => [`\`${p}\``, d.buildings.find((b) => b.prefab === p).type, n])));
  L.push("");
  return L.join("\n");
}
