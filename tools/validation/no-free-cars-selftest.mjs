// Red-proof for validate-no-free-cars: an unlocked vanilla car, an LF car with the lock off and a car with the lock
// written but not set are caught; a locked car, a parked-car marker and a cash-van box prop are not.
// inputs: imports
import { freeCars } from "./validate-no-free-cars.mjs";

const lock = (on) => ` components {\n  LF_VehicleLockComponent "{54D6FBC0EA806FE5}" {\n   m_bDebugLocked ${on}\n  }\n }\n`;
const layer = [
  `GenericEntity A : "{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et" {\n coords 1 2 3\n}`,
  `Vehicle B : "{E1A105BE1A000001}Prefabs/Vehicles/Wheeled/S105/EL_S105_beige.et" {\n coords 1 2 3\n}`,
  `GenericEntity C : "{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et" {\n${lock(0)} coords 1 2 3\n}`,
  `GenericEntity D : "{4A59DAEFE645E8A0}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_tanker_CIV_orange.et" {\n${lock(1)} coords 1 2 3\n}`,
  `GenericEntity E : "{C48202E5BB6761C0}Prefabs/Systems/Vehicles/LF_ParkedCarMarker.et" {\n coords 1 2 3\n}`,
  `GenericEntity F : "{8FABDE609C994BC2}Prefabs/Vehicles/LF_CashVanBox.et" {\n coords 1 2 3\n}`,
].join("\n") + "\n";
const names = freeCars(layer).map((h) => h.name).join(",");
if (names !== "A,B,C") {
  console.log(`FAIL no-free-cars-selftest: expected A,B,C got ${names}`);
  process.exit(1);
}
console.log("no-free-cars-selftest: OK");
