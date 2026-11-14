// tools\cli building: a committed spec -> ArchBuilder in headless Blender -> a building in the addon.
// Playbook: docs/building-pipeline/README.md.
export const help = "building run <slug> [--from s] [--only s] [--force] | status | doctor | selftest   spec -> ArchBuilder -> addon building (docs/building-pipeline/README.md)";

export async function main(args, { workbench } = {}) {
  return (await import("../buildings/pipeline/building.mjs")).main(args, { workbench });
}
