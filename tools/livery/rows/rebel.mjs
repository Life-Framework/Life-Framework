// tools/livery/rows/rebel.mjs - the rebel livery rows (WO-L7, #4471). The table reads every rows/*.mjs
// (tools/livery/table.mjs); the format of a row is documented there. Rebels drive plain olive trucks under
// a coat of mud: the vanilla body material already carries the mud and dirt layers, so a row only repaints
// the colours. No lettering; the UAZ's front doors carry a hand-painted number on a patch of dirty paint.
const OLIVE = [0.105, 0.115, 0.04];
const MUD = [0.085, 0.06, 0.035];
const DERIVED = (what) => ({
  source: `Derived from Bohemia Interactive ${what}`,
  license: "Arma Public License, base-game asset",
});

export default [
  {
    id: "Rebel",
    vehicle: "UAZ469",
    base: "{E1A469BE1A000001}Prefabs/Vehicles/Wheeled/UAZ469/EL_UAZ469_covered_CIV_blue.et",
    colors: { 1: OLIVE, 2: MUD },
    plate: { text: "14", paint: [0.42, 0.4, 0.3], ink: [0.04, 0.035, 0.025], rim: [0.42, 0.4, 0.3] },
    credit: { ...DERIVED("`UAZ469_Body_CIV_BLUE.emat` and `SignWall_01_90x45`"), purpose: "Rebel mud-and-olive repaint and hand-painted door number for the UAZ469" },
  },
  {
    id: "Rebel",
    vehicle: "Ural4320",
    base: "{2A84E52C3429DF4F}Prefabs/Vehicles/LF_Ural4320_RebelKit.et",
    colors: { 1: OLIVE, 2: MUD },
    credit: { ...DERIVED("`Ural_4320_Body_CIV_Forest.emat`, `SignWall_01_90x45`, `CamoNet_PalletMRE`, `CamoNet_PalletAmmo_Soviet` and the camo-net materials CS1"), purpose: "Rebel mud-and-olive repaint for the Ural4320, with a hand-painted door number and camo nets over the bed (LF_Ural4320_RebelKit)" },
  },
];
