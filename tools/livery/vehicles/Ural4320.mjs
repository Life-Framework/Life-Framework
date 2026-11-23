// tools/livery/vehicles/Ural4320.mjs - what the generator knows about the vanilla Ural4320 (the file name is the
// vehicle key a row names; #4471, and WO-L6's company repaint reuses it). The mesh, assign and base material
// are the vanilla Ural4320_transport_CIV_forest body slot: the cab and chassis share the one `Ural_Body`
// material, and the cargo bed is a separate part (VehParts/Ural4320_cargo.et) that keeps its own vanilla look.
// The base is the forest variant because it already carries the mud and dirt layers a rebel truck wants; a row
// overrides only the paint colours. The doors are animated parts of the body mesh, so there are no door parts
// and no plate positions (a row with `plate` fails for this vehicle).
export default {
  baseEmat: "{3AFAAF8DCC91D45D}Assets/Vehicles/Wheeled/Ural4320/Data/Ural_4320_Body_CIV_Forest.emat",
  mesh: "51DAA09FEFBFC0E7",
  assign: "5E2088C27CE01FFC",
  source: "Ural_Body",
  doors: null,
};
