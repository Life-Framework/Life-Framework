// tools/livery/rows/ambulance.mjs - the ambulance livery rows (WO-L5, #4470). The table reads every rows/*.mjs
// (tools/livery/table.mjs); the format of a row is documented there. Medics drive white vans. The generator paints
// the body colours; the red stripe and the AMBULANCE plates sit on the fixed side panels as plate children
// added to each generated variant prefab (neither van has door parts to carry a `plate`).
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
    credit: { ...DERIVED("`uaz452_body_VAI.emat` and `SignWall_01_90x45`"), purpose: "White ambulance repaint and AMBULANCE side plates for the UAZ452" },
  },
  {
    id: "Ambulance",
    vehicle: "S1203",
    base: "{543799AC5C52989C}Prefabs/Vehicles/Wheeled/S1203/S1203_transport_beige.et",
    colors: { 1: WHITE },
    enabled: { 2: false },
    credit: { ...DERIVED("`S1203_body.emat` and `SignWall_01_90x45`"), purpose: "White ambulance repaint with red stripe and AMBULANCE side plates for the S1203" },
  },
];
