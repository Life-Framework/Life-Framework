// tools/livery/vehicles/UAZ452.mjs - what the generator knows about the vanilla UAZ452 van (the file name is the
// vehicle key a row names; the ambulance repaint, #4470). The mesh and assign are the vanilla USSR_VAI
// ambulance's body slot (`uaz452_body`), and the base material is that variant's `uaz452_body_VAI.emat`. Its doors
// are animated parts of the body mesh, so there are no door parts and no plate positions (a row with `plate`
// fails for this vehicle); lettering goes on fixed side panels through the row's `panels` (#4559).
export default {
  baseEmat: "{1818D82250F0F394}Assets/Vehicles/Wheeled/UAZ452/Data/uaz452_body_VAI.emat",
  mesh: "51DAA09FEFBFC0E7",
  assign: "6502DC1DCDB16EF9",
  source: "uaz452_body",
  doors: null,
};
