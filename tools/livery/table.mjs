// tools/livery/table.mjs - the livery table `tools\cli livery build` reads (WO-L2, #4232).
//
// One row is one repainted variant:
//   id        PascalCase name (Police, Ambulance, ...); the files are LF_<Vehicle>_<id>
//   vehicle   a key of VEHICLES in generate.mjs (UAZ469, S1203)
//   base      the {GUID}path prefab the variant inherits (the service prefab, or a vanilla variant)
//   colors    { 1: [r, g, b], 2: ..., 3: ..., 4: ... } paint layer colours, linear 0..1
//   enabled   { 2: false } turns a vanilla paint layer off (the S1203 base mask speckles under layer 2)
//   lettering { mode: "tiled", text, height } repeats text over the mask in colour 2, or
//             { mode: "panels", items: [{ panel, text }] } puts it on measured panels (panels.mjs)
//   credit    { source, license, purpose }: required on every row; it becomes the docs/credits.md row
//
// The WO-L4..L6 cards add their rows here. Colours are what this server is, so the files go to the pack.
const WHITE = [0.62, 0.62, 0.6];
const NAVY = [0.012, 0.03, 0.09];
const DERIVED = (what) => ({
  source: `Derived from Bohemia Interactive ${what}`,
  license: "Arma Public License, base-game asset",
});

export const LIVERIES = [
  {
    id: "Police",
    vehicle: "UAZ469",
    base: "{EFEB161F9BC2031D}Prefabs/Vehicles/Wheeled/UAZ469/LF_UAZ469_Police.et",
    colors: { 1: WHITE, 2: NAVY },
    lettering: { mode: "tiled", text: "POLICE", height: 14 },
    credit: { ...DERIVED("`UAZ469_Body_CIV_BLUE.emat` and `UAZ469_Body_CIV_GLOBAL_MASK.edds`"), purpose: "Police repaint and lettering mask for the UAZ469" },
  },
  {
    id: "Police",
    vehicle: "S1203",
    base: "{543799AC5C52989C}Prefabs/Vehicles/Wheeled/S1203/S1203_transport_beige.et",
    colors: { 1: WHITE },
    enabled: { 2: false },
    credit: { ...DERIVED("`S1203_body.emat`"), purpose: "Police repaint for the S1203" },
  },
];
