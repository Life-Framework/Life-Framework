// Roadmap street visual report. Pure checks live here so selftest can fail an
// empty look and an 80 m outlier with no play session. The runner stands, looks,
// shoots, and writes agent-box/reports/roadmap.html.

import { copyFileSync, mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { capturePixels, classifyMeshFile, samePixels } from "./bmp.mjs";
import { loadPlaced } from "./layers.mjs";
import { judgeTownRow, missingActions, nearestNeighbour, resolveSite, signIssues } from "./town.mjs";

export const STREET_GAP_M = 30;

// The village main street runs east-west on this centre line
// (docs/roadmap/m1-25-village.md). Sites face it from both sides.
export const STREET_Z = 250;

// Sites that are out of town by design: no 30 m street rule, not in the wide shot.
export const OUT_OF_TOWN = ["Roadmap_Convoy_Start"];

// Vanilla prison doors are near-white. The interaction is real, so a white
// center on these two sites is painted-light when look offers an action.
export const PAINTED_LIGHT = ["Roadmap_Jail_Door", "Roadmap_Jail_Cell"];

const MENU_ACTION = /Menu|Dialog|Shop|Catalog|Board|Desk/;

export function roadmapSites(root) {
  return loadPlaced(root).filter((site) => site.name.startsWith("Roadmap_"));
}

export function nearestStreetDistance(index, sites) {
  const site = sites[index];
  let best = Infinity;
  let others = 0;
  for (let i = 0; i < sites.length; i++) {
    if (i === index) continue;
    others++;
    const other = sites[i];
    const distance = Math.hypot(other.x - site.x, other.z - site.z);
    if (distance < best) best = distance;
  }
  if (!others || !Number.isFinite(best)) return 0;
  return Math.round(best * 1000) / 1000;
}

export function selectRoadmapSites(sites, names) {
  if (!names || !names.length) return sites;
  const want = new Set(names);
  return sites.filter((site) => want.has(site.name));
}

export function missingRoadmapNames(sites, names) {
  if (!names || !names.length) return [];
  return names.filter((name) => !sites.some((site) => site.name === name));
}

export function presentedMesh(name, mesh, look) {
  const text = (look ?? "").trim();
  const hasAction = Boolean(text) && text !== "none";
  if (mesh === "white" && hasAction && PAINTED_LIGHT.includes(name)) return "painted-light";
  return mesh || "missing";
}

export function menuActionFromLook(detail) {
  const text = (detail ?? "").trim();
  if (!text || text === "none") return "";
  for (const part of text.split(",")) {
    const hit = part.trim();
    if (!hit) continue;
    const className = hit.split("@")[0];
    if (MENU_ACTION.test(className)) return hit;
  }
  return "";
}

// The character's own loot and confiscate actions have no site name. A site
// passes only when look names that site.
export function siteLook(name, look) {
  const text = (look ?? "").trim();
  if (!text || text === "none") return "";
  if (!name) return text;
  const hits = [];
  for (const part of text.split(",")) {
    const hit = part.trim();
    const at = hit.lastIndexOf("@");
    if (at >= 0 && hit.slice(at + 1) === name) hits.push(hit);
  }
  return hits.join(",");
}

// The capture camera aims at the site from target + offset. A site on the
// street faces it, so the close picture stands in the street: north-side sites
// are shot from the south, south-side sites from the north. Anything else
// (jail yard, hideout, outskirts) keeps the harness default.
export function siteShotOffset(name, site) {
  if (!site || Math.abs(site.z - STREET_Z) > 15) return null;
  const towardStreet = site.z > STREET_Z ? -9 : 9;
  return { ox: 4, oy: 2.2, oz: towardStreet };
}

export function judgeRoadmapRow(row) {
  const failures = [];
  const look = siteLook(row.name, row.look);
  if (!look || look === "none") failures.push("look offers no interaction");
  if (row.menu != null) {
    const menu = String(row.menu).trim();
    const widgets = Boolean(menu) && menu !== "none" && menu !== "blank";
    const shot = row.menuMesh == null || row.menuMesh === "ok" || row.menuMesh === "painted-light";
    if (!widgets || !shot) failures.push("menu is blank");
  }
  if (row.mesh === "white" || row.mesh === "missing") failures.push("mesh is white or missing");
  if (Number(row.distanceM) > STREET_GAP_M && !OUT_OF_TOWN.includes(row.name)) failures.push("more than 30 m from the rest of the street");
  return { ok: failures.length === 0, failures };
}

export function judgeStreet(rows) {
  return rows.map((row, index) => {
    const distanceM = nearestStreetDistance(index, rows);
    const judgment = judgeRoadmapRow({ ...row, distanceM });
    return { ...row, distanceM, ok: judgment.ok, failures: judgment.failures };
  });
}

export function rowCamera(sites, crowd) {
  if (!sites.length) return { x: 0, y: 1, z: 0, ox: 0, oy: 40, oz: 90 };
  let minX = Infinity;
  let maxX = -Infinity;
  let minZ = Infinity;
  let maxZ = -Infinity;
  let y = 0;
  for (const site of sites) {
    minX = Math.min(minX, site.x);
    maxX = Math.max(maxX, site.x);
    minZ = Math.min(minZ, site.z);
    maxZ = Math.max(maxZ, site.z);
    y += site.y;
  }
  const spanX = Math.max(18, maxX - minX);
  const spanZ = Math.max(8, maxZ - minZ);
  const span = Math.max(spanX, spanZ);
  const back = Math.max(40, span * 0.85);
  const up = Math.max(18, span * 0.35);
  const alongX = spanX >= spanZ;
  let ox = alongX ? 0 : back;
  let oy = up;
  let oz = alongX ? back : 0;
  // The player camera faces -Z. A row south of the street is photographed
  // from the north, just short of that street, so the street stays behind the camera.
  if (crowd && crowd.length) {
    const names = new Set(sites.map((site) => site.name));
    let sum = 0;
    let count = 0;
    for (const other of crowd) {
      if (names.has(other.name)) continue;
      sum += alongX ? other.z : other.x;
      count++;
    }
    if (count) {
      const row = alongX ? (minZ + maxZ) / 2 : (minX + maxX) / 2;
      if (sum / count > row + 1) {
        if (alongX) oz = Math.min(back, 12);
        else ox = Math.min(back, 12);
        oy = Math.min(up, 3);
      }
    }
  }
  return {
    x: (minX + maxX) / 2,
    y: y / sites.length,
    z: (minZ + maxZ) / 2,
    ox,
    oy,
    oz,
  };
}

function escapeHtml(value) {
  return String(value ?? "")
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;");
}

function picture(src, alt) {
  if (!src) return "<p class=\"missing\">No picture.</p>";
  return `<img src="${escapeHtml(src)}" alt="${escapeHtml(alt)}">`;
}

export function renderRoadmapReport({ wide, rows, note, cameraStuck, title = "Roadmap street" }) {
  const failed = rows.filter((row) => !row.ok).length;
  const body = rows
    .map((row) => {
      const reasons = row.ok ? "<p class=\"pass\">Pass.</p>" : `<ul class="fail">${row.failures.map((item) => `<li>${escapeHtml(item)}</li>`).join("")}</ul>`;
      const menu = row.menu == null ? "" : `<p>Menu: ${escapeHtml(row.menu)}</p>`;
      const warn = row.warnings?.length ? `<ul class="warn">${row.warnings.map((item) => `<li>${escapeHtml(item)}</li>`).join("")}</ul>` : "";
      const extra = (row.details || []).map((item) => `<p>${escapeHtml(item)}</p>`).join("");
      const menuShot = row.menuSrc ? picture(row.menuSrc, `${row.name} menu`) : "";
      return `<article class="${row.ok ? "pass-row" : "fail-row"}">
  <div class="pictures">
    ${picture(row.src, row.name)}
    ${menuShot}
  </div>
  <div class="text">
    <h2>${escapeHtml(row.name)}</h2>
    <p>${escapeHtml(row.layer || "")} at ${escapeHtml(row.x)} ${escapeHtml(row.y)} ${escapeHtml(row.z)}</p>
    <p>Look: ${escapeHtml(row.look || "none")}</p>
    ${menu}
    ${extra}
    <p>${escapeHtml(row.distanceM)} m from the nearest other site. Mesh: ${escapeHtml(row.mesh || "missing")}.</p>
    ${reasons}
    ${warn}
  </div>
</article>`;
    })
    .join("\n");

  const wideNote = wide?.mesh && wide.mesh !== "ok" ? `<p class="fail">Wide shot: mesh is ${escapeHtml(wide.mesh)}.</p>` : "";
  return `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>${escapeHtml(title)}</title>
<style>
  body { margin: 24px; font: 16px/1.45 Georgia, serif; background: #f4f1ea; color: #1c1915; }
  h1 { font-size: 1.6rem; margin-bottom: 0.2rem; }
  .wide img, .pictures img { width: 100%; height: auto; background: #ddd; }
  article { display: grid; grid-template-columns: minmax(240px, 420px) 1fr; gap: 20px; align-items: start; margin: 28px 0; padding-top: 20px; border-top: 1px solid #d5cfc3; }
  .fail { color: #8c1d1d; }
  .pass { color: #1d5c32; }
  .warn { color: #7a5500; }
  .fail-row { background: #f8e8e4; }
  .missing { color: #8c1d1d; }
</style>
</head>
<body>
<h1>${escapeHtml(title)}</h1>
<p>${rows.length - failed} passed, ${failed} failed, ${rows.length} sites.</p>
<p>${escapeHtml(note || "")}</p>
${cameraStuck ? `<p class="fail">camera_stuck</p>` : ""}
<figure class="wide">
  ${picture(wide?.src, "Roadmap row")}
  <figcaption>${title === "Roadmap street" ? "The row." : "The town."}</figcaption>
</figure>
${wideNote}
${body}
</body>
</html>
`;
}

function publishShot(src, shotDir, fileName) {
  if (!src) return "";
  const dest = join(shotDir, fileName);
  copyFileSync(src, dest);
  return `shots/${fileName}`;
}

function lookText(result) {
  if (!result || result.status !== "ok") return "";
  return result.detail || "";
}

function sessionDown(result) {
  return Boolean(result) && result.detail === "no_session";
}

function publishAndRead(src, shotDir, fileName) {
  if (!src) return { src: "", pixels: null };
  const rel = publishShot(src, shotDir, fileName);
  return { src: rel, pixels: capturePixels(join(shotDir, fileName)) };
}

export function runRoadmapReport(deps) {
  const catalog = deps.sites();
  const names = deps.names || [];
  const missing = missingRoadmapNames(catalog, names);
  if (missing.length) {
    deps.log(`missing site: ${missing.join(", ")}`);
    return 1;
  }
  const sites = selectRoadmapSites(catalog, names);
  const outDir = join(deps.root, "agent-box", "reports");
  const shotDir = join(outDir, "shots");
  mkdirSync(shotDir, { recursive: true });

  let previousPixels = null;
  let cameraStuck = false;
  let playDown = false;

  function remember(pixels) {
    if (!pixels) return false;
    if (previousPixels && samePixels(previousPixels, pixels)) {
      cameraStuck = true;
      return true;
    }
    previousPixels = pixels;
    return false;
  }

  const wide = { src: "", mesh: "missing", detail: "" };
  const rows = [];
  function takeWide() {
    if (!sites.length || cameraStuck || playDown) return;
    const inTown = sites.filter((site) => !OUT_OF_TOWN.includes(site.name));
    const camera = rowCamera(inTown.length ? inTown : sites, catalog);
    try {
      const shot = deps.captureWorld({
        name: "roadmap_row",
        x: camera.x,
        y: camera.y,
        z: camera.z,
        ox: camera.ox,
        oy: camera.oy,
        oz: camera.oz,
      });
      wide.detail = `${shot.status} ${shot.detail || ""}`.trim();
      if (sessionDown(shot)) playDown = true;
      const image = shot.status === "ok" ? deps.resolve(shot) : "";
      if (image) {
        wide.mesh = classifyMeshFile(image);
        const saved = publishAndRead(image, shotDir, "roadmap-row.png");
        wide.src = saved.src;
        remember(saved.pixels);
      }
    } catch (err) {
      wide.detail = err.message;
    }
  }

  if (!playDown && !cameraStuck) {
    for (const site of sites) {
      const row = {
        name: site.name,
        layer: site.layer,
        x: site.x,
        y: site.y,
        z: site.z,
        look: "",
        menu: null,
        menuMesh: null,
        menuSrc: "",
        src: "",
        mesh: "missing",
      };
      try {
        const stood = deps.stand(site.name);
        if (sessionDown(stood)) {
          playDown = true;
          row.note = "no_session";
          rows.push(row);
          break;
        }
        if (stood.status === "ok") deps.pause(800);
        let look = deps.look();
        if (sessionDown(look)) {
          playDown = true;
          row.note = "no_session";
          rows.push(row);
          break;
        }
        // Stand replicates after the wide shot. The character's own actions
        // show up first; wait until this site is in the list.
        let waits = 0;
        while (
          stood.status === "ok" &&
          look.status === "ok" &&
          !siteLook(site.name, look.detail) &&
          waits < 4
        ) {
          deps.pause(700);
          look = deps.look();
          if (sessionDown(look)) {
            playDown = true;
            break;
          }
          waits++;
        }
        if (playDown) {
          row.note = "no_session";
          rows.push(row);
          break;
        }
        row.look = lookText(look);
        const shotSpec = { name: site.name, x: site.x, y: site.y, z: site.z };
        const offset = siteShotOffset(site.name, site);
        if (offset) {
          shotSpec.ox = offset.ox;
          shotSpec.oy = offset.oy;
          shotSpec.oz = offset.oz;
        }
        const shot = deps.captureWorld(shotSpec);
        if (sessionDown(shot)) {
          playDown = true;
          row.note = "no_session";
          rows.push(row);
          break;
        }
        const image = shot.status === "ok" ? deps.resolve(shot) : "";
        let stuck = false;
        if (image) {
          row.mesh = classifyMeshFile(image);
          const saved = publishAndRead(image, shotDir, `${site.name}.png`);
          row.src = saved.src;
          stuck = remember(saved.pixels);
        }
        row.mesh = presentedMesh(row.name, row.mesh, row.look);
        if (!stuck) {
          const menuHit = menuActionFromLook(row.look);
          if (menuHit) {
            const className = menuHit.split("@")[0];
            const entityName = menuHit.split("@")[1] || site.name;
            deps.use(className, entityName);
            deps.pause(500);
            const ui = deps.ui();
            row.menu = ui.status === "ok" ? ui.detail || "none" : "none";
            const menuShot = deps.captureUi(`${site.name}_menu`);
            const menuImage = menuShot.status === "ok" ? deps.resolve(menuShot) : "";
            if (menuImage) {
              row.menuMesh = classifyMeshFile(menuImage);
              row.menuSrc = publishShot(menuImage, shotDir, `${site.name}-menu.png`);
            } else {
              row.menuMesh = "missing";
            }
            deps.input("MenuBack");
            deps.pause(300);
          }
        }
        rows.push(row);
        if (stuck) break;
      } catch (err) {
        row.look = row.look || "";
        row.note = err.message;
        row.mesh = presentedMesh(row.name, row.mesh, row.look);
        rows.push(row);
      }
      if (row.note) deps.log(`${site.name}: ${row.note}`);
    }
  }

  takeWide();

  const judged = judgeStreet(rows);
  const html = renderRoadmapReport({
    wide,
    rows: judged,
    cameraStuck,
    note: "Play-mode street pass. Pictures sit beside the look text. The test server stays down.",
  });
  const htmlPath = join(outDir, "roadmap.html");
  writeFileSync(htmlPath, html, "utf8");
  const failedRows = judged.filter((row) => !row.ok).length;
  const wideFailed = sites.length > 0 && wide.mesh !== "ok";
  const failed = failedRows + (wideFailed ? 1 : 0) + (cameraStuck ? 1 : 0) + (playDown ? 1 : 0);
  deps.log(`report: ${htmlPath}`);
  for (const row of judged) {
    const mark = row.ok ? "pass" : "fail";
    deps.log(`${mark} ${row.name}: ${row.ok ? row.mesh : row.failures.join("; ")}`);
  }
  if (wideFailed) deps.log(`fail roadmap_row: mesh is ${wide.mesh}`);
  if (cameraStuck) deps.log("camera_stuck");
  if (playDown) deps.log("no_session");
  deps.log(`roadmap report: passed=${judged.length - failedRows} failed=${failed} total=${judged.length}`);
  return failed > 0 || sites.length === 0 ? 1 : 0;
}

// Town pass: report roadmap over the manifest (agent-box/town.mjs). Stands at
// each named site, waits for its expected actions, shoots it, opens its menu,
// then stands at its sign and opens Read. Positions come from deps.world(), the
// layer scan; the manifest only names what must exist.
export function runTownReport(deps) {
  const manifest = deps.manifest;
  const world = deps.world();
  const ids = deps.ids || [];
  const unknown = ids.filter((id) => !manifest.sites.some((site) => site.id === id));
  if (unknown.length) {
    deps.log(`unknown site id: ${unknown.join(", ")}`);
    return 1;
  }
  const all = manifest.sites.map((site) => ({ site, place: resolveSite(site, world) }));
  const resolved = all.filter((item) => item.place.x != null);
  const walk = ids.length ? all.filter((item) => ids.includes(item.site.id)) : all;
  const outDir = join(deps.root, "agent-box", "reports");
  const shotDir = join(outDir, "shots");
  mkdirSync(shotDir, { recursive: true });

  let previousPixels = null;
  let cameraStuck = false;
  let playDown = false;
  const rows = [];

  function down(result) {
    if (sessionDown(result)) playDown = true;
    return playDown;
  }

  function lookFor(entityName, expected) {
    let look = deps.look();
    for (let waits = 0; !down(look) && look.status === "ok" && missingActions(expected, entityName, look.detail).length && waits < 4; waits++) {
      deps.pause(700);
      look = deps.look();
    }
    return look;
  }

  for (const { site, place } of walk) {
    if (playDown || cameraStuck) break;
    const row = {
      id: site.id,
      name: site.entity ? `${site.id} (${site.entity})` : `${site.id} (${place.count || 0} x ${site.prefab})`,
      entity: site.entity || "",
      actions: site.actions,
      outskirts: Boolean(site.outskirts),
      layer: place.layer || "",
      x: place.x ?? "",
      y: place.y ?? "",
      z: place.z ?? "",
      look: "",
      menu: null,
      menuMesh: null,
      menuSrc: "",
      src: "",
      mesh: "missing",
      resolveErrors: place.errors,
      details: [`Journey step ${site.step}. Expects: ${site.actions.join(", ") || "nothing to press"}.`],
    };
    rows.push(row);
    if (place.x == null) continue;
    const index = resolved.findIndex((item) => item.site.id === site.id);
    row.distanceM = nearestNeighbour(index, resolved.map((item) => item.place));
    row.neighbourM = row.distanceM;
    try {
      if (site.entity) {
        const stood = deps.stand(site.entity);
        if (down(stood)) break;
        if (stood.status === "ok") deps.pause(800);
        const look = lookFor(site.entity, site.actions);
        if (playDown) break;
        row.look = lookText(look);
      }
      const shotSpec = { name: `town_${site.id}`, x: place.x, y: place.y, z: place.z };
      const offset = site.entity ? siteShotOffset(site.entity) : null;
      if (offset) Object.assign(shotSpec, offset);
      const shot = deps.captureWorld(shotSpec);
      if (down(shot)) break;
      const image = shot.status === "ok" ? deps.resolve(shot) : "";
      if (image) {
        row.mesh = classifyMeshFile(image);
        const saved = publishAndRead(image, shotDir, `town-${site.id}.png`);
        row.src = saved.src;
        if (saved.pixels && previousPixels && samePixels(previousPixels, saved.pixels)) cameraStuck = true;
        if (saved.pixels) previousPixels = saved.pixels;
      }
      row.mesh = presentedMesh(site.entity, row.mesh, row.look);
      if (cameraStuck) break;
      const menuHit = site.entity ? menuActionFromLook(siteLook(site.entity, row.look)) : "";
      if (menuHit) {
        deps.use(menuHit.split("@")[0], site.entity);
        deps.pause(500);
        const ui = deps.ui();
        row.menu = ui.status === "ok" ? ui.detail || "none" : "none";
        const menuShot = deps.captureUi(`town_${site.id}_menu`);
        const menuImage = menuShot.status === "ok" ? deps.resolve(menuShot) : "";
        row.menuMesh = menuImage ? classifyMeshFile(menuImage) : "missing";
        if (menuImage) row.menuSrc = publishShot(menuImage, shotDir, `town-${site.id}-menu.png`);
        deps.input("MenuBack");
        deps.pause(300);
      }
      const live = {};
      const offline = signIssues(site, place, world, deps.catalog, deps.strings, manifest.signRadiusM);
      if (offline.sign && offline.sign.name && offline.sign.distanceM <= manifest.signRadiusM) {
        const stood = deps.stand(offline.sign.name);
        if (down(stood)) break;
        if (stood.status === "ok") deps.pause(800);
        const look = lookFor(offline.sign.name, ["LF_SignReadAction"]);
        if (playDown) break;
        live.read = missingActions(["LF_SignReadAction"], offline.sign.name, lookText(look)).length === 0;
        if (live.read) {
          deps.use("LF_SignReadAction", offline.sign.name);
          deps.pause(500);
          const ui = deps.ui();
          const card = deps.captureUi(`town_${site.id}_sign`);
          const cardImage = card.status === "ok" ? deps.resolve(card) : "";
          const cardMesh = cardImage ? classifyMeshFile(cardImage) : "missing";
          if (cardImage) row.signSrc = publishShot(cardImage, shotDir, `town-${site.id}-sign.png`);
          const widgets = ui.status === "ok" && (ui.detail || "").trim() && ui.detail !== "none";
          live.card = widgets && cardMesh === "ok" ? "ok" : "blank";
          deps.input("MenuBack");
          deps.pause(300);
        }
      }
      const signed = signIssues(site, place, world, deps.catalog, deps.strings, manifest.signRadiusM, live);
      row.signIssues = signed.issues;
      if (signed.sign) row.details.push(`Sign ${site.sign}: ${signed.sign.name || "(unnamed)"} ${signed.sign.distanceM} m away.`);
      else if (site.sign) row.details.push(`Sign ${site.sign}: none placed.`);
    } catch (err) {
      row.note = err.message;
      deps.log(`${site.id}: ${err.message}`);
    }
  }

  const judged = rows.map((row) => {
    const verdict = judgeTownRow(row, manifest);
    return { ...row, ok: verdict.ok, failures: verdict.failures, warnings: verdict.warnings };
  });
  const html = renderRoadmapReport({
    wide: {},
    rows: judged.map((row) => ({ ...row, menuSrc: row.menuSrc || row.signSrc || "" })),
    cameraStuck,
    title: "Town",
    note: `Play-mode town pass over agent-box/town.mjs. requireSigns=${manifest.requireSigns}: sign problems are ${manifest.requireSigns ? "failures" : "warnings (sign: pending)"}.`,
  });
  const htmlPath = join(outDir, "town.html");
  writeFileSync(htmlPath, html, "utf8");
  const failedRows = judged.filter((row) => !row.ok).length;
  const failed = failedRows + (cameraStuck ? 1 : 0) + (playDown ? 1 : 0);
  deps.log(`report: ${htmlPath}`);
  for (const row of judged) {
    deps.log(`${row.ok ? "pass" : "fail"} ${row.id}: ${row.ok ? row.mesh : row.failures.join("; ")}`);
    for (const warning of row.warnings) deps.log(`warn ${row.id}: ${warning}`);
  }
  if (cameraStuck) deps.log("camera_stuck");
  if (playDown) deps.log("no_session");
  deps.log(`town report: passed=${judged.length - failedRows} failed=${failed} total=${judged.length}`);
  return failed > 0 || judged.length === 0 ? 1 : 0;
}
