// Writes the paint overlays the uniform specs lay over the vanilla maps (docs/asset-pipeline/clothing.md "Spec").
//   node tools/outfits/uniform-paint/paint.mjs shirt|cap|calib <out.png>
// The overlays are RGBA PNGs on the vanilla UV layout, 2048 x 2048. Insignia are invented: a gold
// seven-point star in a ring, no real-world force's mark.
import { Canvas, number } from "./png.mjs";

const GOLD = [214, 172, 52], DARK = [18, 26, 48];

function badge(cv, cx, cy, r, rot) {
  cv.disc(cx, cy, r, GOLD);
  cv.disc(cx, cy, r * 0.86, DARK);
  cv.star(cx, cy, r * 0.8, 7, GOLD, 1, 0.5, rot);
  cv.disc(cx, cy, r * 0.22, DARK);
}

const [, , mode, out] = process.argv;
const cv = new Canvas(mode === "shirt" ? 2048 : Number(process.env.SIZE || 1024));
if (mode === "shirt") {
  // The wearer's left chest panel is turned 90 degrees in the atlas: texture x runs down the body.
  badge(cv, 1290, 860, 64, Math.PI);
} else if (mode === "cap") {
  // The vanilla cap's cockade and its two buttons carry a Soviet star: all three are painted over.
  // Positions are on the 1024 map (the cap's own layout, read from the paint template).
  badge(cv, 918, 838, 98, -Math.PI / 2);
  for (const [x, y] of [[975, 615], [970, 712]]) {
    cv.disc(x, y, 52, GOLD);
    cv.disc(x, y, 40, [186, 146, 40]);
    cv.ring(x, y, 22, 26, GOLD);
  }
} else if (mode === "calib") {
  const hues = [[230, 60, 60], [60, 170, 70], [60, 90, 230], [220, 180, 40]];
  const q = cv.w / 4;
  for (let r = 0; r < 4; r++) for (let c = 0; c < 4; c++) {
    cv.rect(c * q + 4, r * q + 4, q - 8, q - 8, hues[(c + r) % 4], 0.85);
    number(cv, r * 4 + c + 1, c * q + q * 0.23, r * q + q * 0.2, q * 0.55, [255, 255, 255]);
  }
} else throw new Error("mode: shirt|cap|calib");
cv.save(out);
