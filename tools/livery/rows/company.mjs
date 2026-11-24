// tools/livery/rows/company.mjs - the company livery rows (WO-L6, #4472). The table reads every rows/*.mjs
// (tools/livery/table.mjs); the format of a row is documented there. Island companies run their own colours:
// a yellow taxi with a roof sign, a cream bakery van, a red hardware van, a blue factory van and a green timber
// truck. The generator paints the body colours; the company name sits on a plate on each side (`panels`, #4559),
// written by the build as children of each variant prefab (none of these bodies has door parts to carry a
// `plate`). The names are invented, no real brand, and all lettering is US English. Each plate's sign row and
// string are hand-written in the pack (Configs/Signs/LF_Signs_Everon1989/LF_Signs_Livery_<Name>.conf), like the
// ambulance's.
const DERIVED = (what) => ({
  source: `Derived from Bohemia Interactive ${what}`,
  license: "Arma Public License, base-game asset",
});

const CREAM_PLATE = [0.95, 0.93, 0.86];
const BLACK = [0.03, 0.03, 0.03];

export default [
  {
    // The island taxi: the locked beige S105 in cab yellow. TAXI on both rear quarter panels (the same flush,
    // tilted fit as the police plates) and a roof sign, two plates back to back, read from either side.
    id: "Taxi",
    vehicle: "S105",
    base: "{E1A105BE1A000001}Prefabs/Vehicles/Wheeled/S105/EL_S105_beige.et",
    colors: { 1: [0.56, 0.38, 0.02] },
    panels: {
      sign: "livery_company_taxi",
      paint: [0.97, 0.78, 0.1], ink: BLACK, rim: BLACK,
      at: [
        { name: "TaxiLetters_L", coords: [-0.83, 0.82, -1.3], yaw: -88.5, scale: 0.5, tilt: -18 },
        { name: "TaxiLetters_R", coords: [0.83, 0.82, -1.3], yaw: 88.5, scale: 0.5, tilt: -18 },
        { name: "TaxiRoof_L", coords: [-0.03, 1.4, 0.1], yaw: -90, scale: 0.4 },
        { name: "TaxiRoof_R", coords: [0.03, 1.4, 0.1], yaw: 90, scale: 0.4 },
      ],
    },
    credit: { ...DERIVED("`S105Body_mat_beige.emat` and `SignWall_01_90x45`"), purpose: "Yellow taxi repaint with TAXI quarter-panel and roof plates for the S105" },
  },
  {
    id: "CompanyBakery",
    vehicle: "S1203",
    base: "{543799AC5C52989C}Prefabs/Vehicles/Wheeled/S1203/S1203_transport_beige.et",
    colors: { 1: [0.6, 0.48, 0.28] },
    enabled: { 2: false },
    panels: {
      sign: "livery_company_bakery",
      paint: [0.42, 0.18, 0.08], ink: CREAM_PLATE, rim: CREAM_PLATE,
      at: [
        { name: "CompanyLetters_L", coords: [-0.95, 0.96, -1.2], yaw: -90, scale: 0.7 },
        { name: "CompanyLetters_R", coords: [0.95, 0.96, -1.2], yaw: 90, scale: 0.7 },
      ],
    },
    credit: { ...DERIVED("`S1203_body.emat` and `SignWall_01_90x45`"), purpose: "Cream bakery van repaint with the EVERON BAKERY side plates for the S1203" },
  },
  {
    id: "CompanyFactory",
    vehicle: "S1203",
    base: "{543799AC5C52989C}Prefabs/Vehicles/Wheeled/S1203/S1203_transport_beige.et",
    colors: { 1: [0.04, 0.1, 0.26] },
    enabled: { 2: false },
    panels: {
      sign: "livery_company_factory",
      paint: CREAM_PLATE, ink: [0.04, 0.1, 0.26], rim: [0.04, 0.1, 0.26],
      at: [
        { name: "CompanyLetters_L", coords: [-0.95, 0.96, -1.2], yaw: -90, scale: 0.7 },
        { name: "CompanyLetters_R", coords: [0.95, 0.96, -1.2], yaw: 90, scale: 0.7 },
      ],
    },
    credit: { ...DERIVED("`S1203_body.emat` and `SignWall_01_90x45`"), purpose: "Factory-blue van repaint with the EVERON WORKS side plates for the S1203" },
  },
  {
    id: "CompanyHardware",
    vehicle: "UAZ452",
    base: "{A003493D32140960}Prefabs/Vehicles/Wheeled/UAZ452/UAZ452_cargo_CIV_base.et",
    colors: { 1: [0.5, 0.09, 0.03] },
    panels: {
      sign: "livery_company_hardware",
      paint: CREAM_PLATE, ink: [0.5, 0.09, 0.03], rim: [0.5, 0.09, 0.03],
      at: [
        { name: "CompanyLetters_L", coords: [-0.98, 1.12, -0.9], yaw: -90, scale: 0.7 },
        { name: "CompanyLetters_R", coords: [0.98, 1.12, -0.9], yaw: 90, scale: 0.7 },
      ],
    },
    credit: { ...DERIVED("`uaz452_body_VAI.emat` and `SignWall_01_90x45`"), purpose: "Red hardware van repaint with the EVERON HARDWARE side plates for the UAZ452" },
  },
  {
    // The timber truck: a plain forest-green cab under a clean coat (layer 2 is the vanilla mud, kept dark green
    // rather than brown). The company name rides on both cab doors.
    id: "CompanyTimber",
    vehicle: "Ural4320",
    base: "{2B402516452EC797}Prefabs/Vehicles/Wheeled/Ural4320/LF_Ural4320_transport_CIV.et",
    colors: { 1: [0.05, 0.2, 0.07], 2: [0.03, 0.09, 0.04] },
    panels: {
      sign: "livery_company_timber",
      paint: CREAM_PLATE, ink: [0.05, 0.2, 0.07], rim: [0.05, 0.2, 0.07],
      at: [
        { name: "CompanyLetters_L", coords: [-1.12, 1.35, 1.2], yaw: -90, scale: 0.6 },
        { name: "CompanyLetters_R", coords: [1.12, 1.35, 1.2], yaw: 90, scale: 0.6 },
      ],
    },
    credit: { ...DERIVED("`Ural_4320_Body_CIV_Forest.emat` and `SignWall_01_90x45`"), purpose: "Forest-green timber truck repaint with the EVERON TIMBER cab-door plates for the Ural4320" },
  },
];
