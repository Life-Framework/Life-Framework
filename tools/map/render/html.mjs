// tools/map/render/html.mjs - the hover variant of a render: the PNG embedded as a data URI, with the POIs and
// areas as hover targets and a live world coordinate under the cursor. One self-contained file.
const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" })[c]);

export function buildHtml({ png, view, hits, persona, layer }) {
  const data = JSON.stringify({ hits, view: { x0: view.x0, z1: view.z1, scale: view.scale, w: view.w, h: view.h } });
  return `<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Everon map: ${esc(persona)}</title>
<style>
  :root { --bg: #f4efe2; --ink: #3a2e24; --card: #fffaf0; }
  @media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) { --bg: #1d1a16; --ink: #eadfc8; --card: #2a2620; } }
  body { margin: 0; padding: 16px; background: var(--bg); color: var(--ink); font: 14px/1.4 system-ui, sans-serif; }
  .wrap { position: relative; display: inline-block; max-width: 100%; }
  img { display: block; max-width: 100%; height: auto; }
  svg { position: absolute; inset: 0; width: 100%; height: 100%; }
  .a { fill: transparent; } .a:hover { fill: rgba(60, 40, 20, .12); }
  .p { fill: transparent; stroke: transparent; } .p:hover { stroke: #1b1b1b; stroke-width: 2; }
  #tip { position: fixed; pointer-events: none; display: none; background: var(--card); border: 1px solid var(--ink); padding: 6px 8px; border-radius: 4px; }
  #pos { margin-top: 8px; font-variant-numeric: tabular-nums; }
</style></head><body>
<div class="wrap"><img id="map" alt="Everon map (${esc(persona)}, ${esc(layer)} layer)" src="data:image/png;base64,${png.toString("base64")}">
<svg id="ov" viewBox="0 0 ${view.w} ${view.h}" preserveAspectRatio="none"></svg></div>
<div id="pos">hover the map</div><div id="tip"></div>
<script>
const D = ${data};
const ov = document.getElementById("ov"), tip = document.getElementById("tip"), pos = document.getElementById("pos"), img = document.getElementById("map");
const NS = "http://www.w3.org/2000/svg";
for (const h of D.hits) {
  const e = document.createElementNS(NS, h.kind === "poi" ? "circle" : "polygon");
  e.setAttribute("class", h.kind === "poi" ? "p" : "a");
  if (h.kind === "poi") { e.setAttribute("cx", h.x); e.setAttribute("cy", h.y); e.setAttribute("r", h.r); }
  else e.setAttribute("points", h.pts.map(p => p.join(",")).join(" "));
  e.addEventListener("mousemove", ev => { tip.style.display = "block"; tip.style.left = ev.clientX + 14 + "px"; tip.style.top = ev.clientY + 14 + "px"; tip.textContent = h.label + " - " + h.detail; });
  e.addEventListener("mouseleave", () => { tip.style.display = "none"; });
  ov.appendChild(e);
}
img.parentElement.addEventListener("mousemove", ev => {
  const r = img.getBoundingClientRect(), px = (ev.clientX - r.left) * D.view.w / r.width, py = (ev.clientY - r.top) * D.view.h / r.height;
  pos.textContent = "x " + (D.view.x0 + px / D.view.scale).toFixed(0) + "  z " + (D.view.z1 - py / D.view.scale).toFixed(0);
});
</script></body></html>
`;
}
