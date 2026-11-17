import fs from "node:fs";
const D = "docs/everon";
const groups = ["retail", "office-police", "residential", "medical-services", "signs-lights-street", "vehicles-dock-industrial"];
const t = "`";
const all = [];
const summary = [];
const extra = {};
for (const g of groups) {
  const f = `${D}/props/${g}.json`;
  if (!fs.existsSync(f)) { summary.push({ group: g, missing: true }); continue; }
  let j;
  try { j = JSON.parse(fs.readFileSync(f, "utf8")); } catch (e) { summary.push({ group: g, error: String(e).slice(0, 80) }); continue; }
  const items = j.items || [];
  for (const it of items) all.push({ group: g, ...it });
  const withAction = items.filter((i) => i.hasAction === true).length;
  const verified = items.filter((i) => i.res || i.guid).length;
  summary.push({ group: g, items: items.length, withAction, unresolvedGuid: items.length - verified });
  for (const k of Object.keys(j)) if (!["group", "generated", "items"].includes(k)) (extra[g] ??= {})[k] = j[k];
}
fs.writeFileSync(`${D}/catalog-props.json`, JSON.stringify({ generated: "2026-11-16", note: "Merged from docs/everon/props/<group>.json. hasAction: the prefab itself carries a user action (null = not verified). everonWorldCount: placed instances on vanilla Everon.", groups: summary, extra, items: all }));
let md = `# Everon props catalog

Generated 2026-11-16. The vanilla props most useful for roleplay fit-out, grouped by use. Each group has its own file with per-category tables, sizes (${t}tools\\cli measure${t}), how many exist on vanilla Everon, which towns use them and whether the prefab already carries a user action. Merged data: [catalog-props.json](catalog-props.json) (${all.length} items). Buildings: [catalog-buildings.md](catalog-buildings.md).

| group | file | items | with a user action |
|---|---|---:|---:|
`;
for (const s of summary) md += s.missing || s.error ? `| ${s.group} | (missing) | | |\n` : `| ${s.group} | [props/${s.group}.md](props/${s.group}.md) | ${s.items} | ${s.withAction} |\n`;
md += `
## Reading the flags

- **hasAction / actions**: a user-action entry found in the vanilla prefab (sit, open, pick up, play). A prop with ${t}false${t} is a mesh only: anything interactive must be added by the mod (Life Framework's own sit and bed actions are in ${t}docs/features/${t}).
- **everonWorldCount**: instances placed on vanilla Everon (whole map for non-structure props; structure props counted from the structure dump).
- **usedInTowns**: from the 450 m dumps around Saint Philippe, Saint Pierre, Morton and Montignac only.
- Items whose GUID the pak data does not expose carry null in ${t}guid${t}/${t}res${t}; find them with ${t}tools\\cli call asset_search${t}.
`;
fs.writeFileSync(`${D}/catalog-props.md`, md);
console.log(JSON.stringify(summary));
