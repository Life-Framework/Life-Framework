// tools/livery/vehicles/UAZ469.mjs - what the generator knows about the vanilla UAZ469 (the file name is the
// vehicle key a row names). Mesh and assign GUIDs are the vanilla body MeshObject's (the spike read them from
// the vanilla prefabs; they are the same on every variant). `plate` is where a front-door plate sits in the
// door's own frame (the door .xob's bounds: the outer skin is at x -0.937 / +0.935 and the flat lower panel
// spans y 0.5..1.0, z 0..0.84), and its yaw so the face points out of the door.
export default {
  baseEmat: "{D1F1DD9AE3DC919D}Assets/Vehicles/Wheeled/UAZ469/Data/UAZ469_Body_CIV_BLUE.emat",
  mesh: "51DAA09FEFBFC0E7",
  assign: "64F09BE9246E252A",
  source: "UAZ469_Body",
  doors: {
    vehicle: "Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_covered_CIV_blue.et",
    names: ["L01", "R01", "L02", "R02"],
    door: (d) => `Prefabs/Vehicles/Wheeled/UAZ469/VehParts/Doors/Blue/UAZ469_door_${d}_blue.et`,
    top: (d) => `Prefabs/Vehicles/Wheeled/UAZ469/VehParts/Doors/Blue/UAZ469_door_${d}_top_blue.et`,
    plate: {
      L01: { coords: [-0.94, 0.78, 0.42], yaw: -90 },
      R01: { coords: [0.938, 0.78, 0.42], yaw: 90 },
    },
    plateScale: 0.6,
  },
};
