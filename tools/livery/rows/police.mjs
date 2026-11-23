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
    // The core police base (#4606) holds the car radio, unit status and Run plate rows, so a rebuild keeps them.
    base: "{244589E898866C68}Prefabs/Vehicles/Wheeled/S105/LF_S105_PoliceBase.et",
    colors: { 1: WHITE },
    // POLICE on the fixed rear quarter panels (#4372): the doors are part of the body mesh, so no door plate.
    // Flush with the paint (#4559): the body is 0.80 m out at the plate (traced), leans in 18 degrees at the top and
    // tapers 2.5 cm toward the tail, so the plate is tilted -18 and toed 1.5 degrees, between the arch and the crease.
    panels: {
      sign: "livery_uaz469_police",
      paint: [0.95, 0.95, 0.93], ink: [0.05, 0.1, 0.35], rim: [0.05, 0.1, 0.35],
      at: [
        { name: "PoliceLetters_L", coords: [-0.83, 0.82, -1.3], yaw: -88.5, scale: 0.5, tilt: -18 },
        { name: "PoliceLetters_R", coords: [0.83, 0.82, -1.3], yaw: 88.5, scale: 0.5, tilt: -18 },
      ],
    },
    // "Run plate" (#4425): the Cadet's car runs plates like the UAZ469 (whose base prefab carries the row itself). The base
    // EL_S105_beige has no police rows, so the build appends it to the ActionsManagerComponent that base declares.
    actions: {
      manager: "C97BE5489221AE18",
      rows: [{ cls: "LF_RunPlateAction", name: "#LF-RunPlate_Action", contexts: ["default"], priority: 11 }],
    },
    credit: { ...DERIVED("`S105Body_mat_beige.emat`"), purpose: "Police repaint for the S105" },
  },
  {
    id: "Police",
    vehicle: "S1203",
    // The core police base (#4606), as for the S105.
    base: "{C1493A67B59073A1}Prefabs/Vehicles/Wheeled/S1203/LF_S1203_PoliceBase.et",
    colors: { 1: WHITE },
    enabled: { 2: false },
    credit: { ...DERIVED("`S1203_body.emat`"), purpose: "Police repaint for the S1203" },
  },
];
