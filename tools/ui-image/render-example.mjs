// node tools/ui-image/render-example.mjs  ->  tmp/ui-image-medic_flow.svg
// The example diagram's source; feed the file to `tools\cli ui-image`.
import { mkdirSync, writeFileSync } from "node:fs";
import { flowSvg } from "./medic-flow.svg.mjs";

mkdirSync("tmp", { recursive: true });
writeFileSync("tmp/ui-image-medic_flow.svg", flowSvg());
console.log("wrote tmp/ui-image-medic_flow.svg");
