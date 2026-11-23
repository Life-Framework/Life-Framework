// tools/livery/rows/police.mjs - the police livery rows (WO-L4, #4232). The table reads every rows/*.mjs
// (tools/livery/table.mjs); the format of a row is documented there.
const WHITE = [0.62, 0.62, 0.6];
const NAVY = [0.012, 0.03, 0.09];
const DERIVED = (what) => ({
  source: `Derived from Bohemia Interactive ${what}`,
  license: "Arma Public License, base-game asset",
});

export default [
  {
    id: "Police",
    vehicle: "UAZ469",
    base: "{EFEB161F9BC2031D}Prefabs/Vehicles/Wheeled/UAZ469/LF_UAZ469_Police.et",
    colors: { 1: WHITE, 2: NAVY },
    plate: { text: "POLICE", paint: [0.95, 0.95, 0.93], ink: [0.05, 0.1, 0.35], rim: [0.05, 0.1, 0.35] },
    credit: { ...DERIVED("`UAZ469_Body_CIV_BLUE.emat` and `SignWall_01_90x45`"), purpose: "Police repaint and door plates for the UAZ469" },
  },
  {
    // The Cadet's car in the rank fleet (#4352): the locked beige S105, repainted white.
    id: "Police",
    vehicle: "S105",
    base: "{E1A105BE1A000001}Prefabs/Vehicles/Wheeled/S105/EL_S105_beige.et",
    colors: { 1: WHITE },
    credit: { ...DERIVED("`S105Body_mat_beige.emat`"), purpose: "Police repaint for the S105" },
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
