// tools/livery/rows/ambulance.mjs - the ambulance livery rows (WO-L5, #4470). The table reads every rows/*.mjs
// (tools/livery/table.mjs); the format of a row is documented there. Medics drive white vans. The generator paints
// the body colours; the AMBULANCE plates sit on the fixed side panels (`panels`, #4559), written by the build as
// children of each variant prefab (neither van has door parts to carry a `plate`).
const WHITE = [0.66, 0.66, 0.64];
const DERIVED = (what) => ({
  source: `Derived from Bohemia Interactive ${what}`,
  license: "Arma Public License, base-game asset",
});

export default [
  {
    id: "Ambulance",
    vehicle: "UAZ452",
    base: "{43C4AF1EEBD001CE}Prefabs/Vehicles/Wheeled/UAZ452/UAZ452_ambulance.et",
    colors: { 1: WHITE },
    panels: {
      sign: "livery_ambulance",
      paint: [0.95, 0.95, 0.93], ink: [0.6, 0.03, 0.03], rim: [0.6, 0.03, 0.03],
      at: [
        { name: "AmbulanceLetters_L", coords: [-0.98, 1.12, -0.9], yaw: -90, scale: 0.7 },
        { name: "AmbulanceLetters_R", coords: [0.98, 1.12, -0.9], yaw: 90, scale: 0.7 },
      ],
    },
    credit: { ...DERIVED("`uaz452_body_VAI.emat` and `SignWall_01_90x45`"), purpose: "White ambulance repaint and AMBULANCE side plates for the UAZ452" },
  },
  {
    id: "Ambulance",
    vehicle: "S1203",
    base: "{543799AC5C52989C}Prefabs/Vehicles/Wheeled/S1203/S1203_transport_beige.et",
    colors: { 1: WHITE },
    enabled: { 2: false },
    panels: {
      sign: "livery_ambulance",
      paint: [0.55, 0.03, 0.03], ink: [0.97, 0.97, 0.95], rim: [0.97, 0.97, 0.95],
      at: [
        { name: "AmbulanceLetters_L", coords: [-0.95, 0.96, -1.2], yaw: -90, scale: 0.7 },
        { name: "AmbulanceLetters_R", coords: [0.95, 0.96, -1.2], yaw: 90, scale: 0.7 },
      ],
    },
    credit: { ...DERIVED("`S1203_body.emat` and `SignWall_01_90x45`"), purpose: "White ambulance repaint with red stripe and AMBULANCE side plates for the S1203" },
  },
];
