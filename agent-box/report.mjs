// Roadmap street visual report. Pure checks live here so selftest can fail an
// empty look and an 80 m outlier with no play session. The runner stands, looks,
// shoots, and writes agent-box/reports/roadmap.html.

import { copyFileSync, mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { capturePixels, classifyMeshFile, samePixels } from "./bmp.mjs";
import { loadPlaced } from "./layers.mjs";

export const STREET_GAP_M = 30;

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

export function judgeRoadmapRow(row) {
  const failures = [];
  const look = (row.look ?? "").trim();
  if (!look || look === "none") failures.push("look offers no interaction");
  if (row.menu != null) {
    const menu = String(row.menu).trim();
    const widgets = Boolean(menu) && menu !== "none" && menu !== "blank";
    const shot = row.menuMesh == null || row.menuMesh === "ok" || row.menuMesh === "painted-light";
    if (!widgets || !shot) failures.push("menu is blank");
  }
  if (row.mesh === "white" || row.mesh === "missing") failures.push("mesh is white or missing");
  if (Number(row.distanceM) > STREET_GAP_M) failures.push("more than 30 m from the rest of the street");
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
  let oz = alongX ? back : 0;
  // A filtered row stands on the open side, so the rest of the street is not
  // between the player and the sites being photographed.
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
        if (alongX) oz = -back;
        else ox = -back;
      }
    }
  }
  return {
    x: (minX + maxX) / 2,
    y: y / sites.length,
    z: (minZ + maxZ) / 2,
    ox,
    oy: up,
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

export function renderRoadmapReport({ wide, rows, note, cameraStuck }) {
  const failed = rows.filter((row) => !row.ok).length;
  const body = rows
    .map((row) => {
      const reasons = row.ok ? "<p class=\"pass\">Pass.</p>" : `<ul class="fail">${row.failures.map((item) => `<li>${escapeHtml(item)}</li>`).join("")}</ul>`;
      const menu = row.menu == null ? "" : `<p>Menu: ${escapeHtml(row.menu)}</p>`;
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
    <p>${escapeHtml(row.distanceM)} m from the nearest other site. Mesh: ${escapeHtml(row.mesh || "missing")}.</p>
    ${reasons}
  </div>
</article>`;
    })
    .join("\n");

  const wideNote = wide?.mesh && wide.mesh !== "ok" ? `<p class="fail">Wide shot: mesh is ${escapeHtml(wide.mesh)}.</p>` : "";
  return `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Roadmap street</title>
<style>
  body { margin: 24px; font: 16px/1.45 Georgia, serif; background: #f4f1ea; color: #1c1915; }
  h1 { font-size: 1.6rem; margin-bottom: 0.2rem; }
  .wide img, .pictures img { width: 100%; height: auto; background: #ddd; }
  article { display: grid; grid-template-columns: minmax(240px, 420px) 1fr; gap: 20px; align-items: start; margin: 28px 0; padding-top: 20px; border-top: 1px solid #d5cfc3; }
  .fail { color: #8c1d1d; }
  .pass { color: #1d5c32; }
  .fail-row { background: #f8e8e4; }
  .missing { color: #8c1d1d; }
</style>
</head>
<body>
<h1>Roadmap street</h1>
<p>${rows.length - failed} passed, ${failed} failed, ${rows.length} sites.</p>
<p>${escapeHtml(note || "")}</p>
${cameraStuck ? `<p class="fail">camera_stuck</p>` : ""}
<figure class="wide">
  ${picture(wide?.src, "Roadmap row")}
  <figcaption>The row.</figcaption>
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
  if (sites.length) {
    const camera = rowCamera(sites, catalog);
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

  const rows = [];
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
        if (stood.status === "ok" && look.status === "ok" && (look.detail || "").trim() === "none") {
          deps.pause(700);
          look = deps.look();
        }
        row.look = lookText(look);
        const shot = deps.captureWorld({ name: site.name, x: site.x, y: site.y, z: site.z });
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
