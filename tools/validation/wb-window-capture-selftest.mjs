// Pixel code behind `tools\cli wb-shot` (tools/lib/wb-window-capture.mjs): padded 24-bit BGR scanlines to
// RGB, crop, box downscale, PNG round trip. The Win32 half needs a live Workbench and is proved by hand
// (docs/setup.md "Workbench window capture").
// inputs: tools/lib/wb-window-capture.mjs, agent-box/bmp.mjs
// red-proof: with the stride line in bgrToRgb changed to `width * 3` it failed with
// "bgrToRgb row 1 pixel 0: got [9,9,9] want [255,255,255]" (GetDIBits pads each row to 4 bytes).
import { bgrToRgb, crop, downscale } from "../lib/wb-window-capture.mjs";
import { decodePng, encodePng } from "../../agent-box/bmp.mjs";

const failures = [];
const eq = (what, a, b) => { if (JSON.stringify(a) !== JSON.stringify(b)) failures.push(`${what}: got ${JSON.stringify(a)} want ${JSON.stringify(b)}`); };

// 3x2 frame: stride 12 (9 data + 3 pad). Row 0: red, green, blue. Row 1: white, black, grey.
const bgr = Buffer.from([
  0, 0, 255, 0, 255, 0, 255, 0, 0, 9, 9, 9,
  255, 255, 255, 0, 0, 0, 128, 128, 128, 9, 9, 9,
]);
const rgb = bgrToRgb(3, 2, bgr);
eq("bgrToRgb row 0", [...rgb.subarray(0, 9)], [255, 0, 0, 0, 255, 0, 0, 0, 255]);
eq("bgrToRgb row 1 pixel 0", [...rgb.subarray(9, 12)], [255, 255, 255]);

const c = crop(3, 2, rgb, { x: 1, y: 1, w: 9, h: 9 });
eq("crop clamps", [c.width, c.height, [...c.rgb]], [2, 1, [0, 0, 0, 128, 128, 128]]);

const big = Buffer.alloc(8 * 4 * 3, 200);
const s = downscale(8, 4, big, 4);
eq("downscale size", [s.width, s.height], [4, 2]);
eq("downscale flat", s.rgb[0], 200);
eq("downscale no-op", downscale(8, 4, big, 0).width, 8);

const back = decodePng(encodePng(3, 2, rgb));
eq("png round trip", [back.width, back.height], [3, 2]);

if (failures.length) {
  console.log(`FAIL  wb-window-capture-selftest\n  ${failures.join("\n  ")}`);
  process.exit(1);
}
console.log("wb-window-capture-selftest: ok");
