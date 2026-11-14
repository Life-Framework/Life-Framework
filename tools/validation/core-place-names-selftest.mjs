// Red-proof for validate-core-place-names: each place name is caught, neutral text is not.
import { placeNameHits } from "./validate-core-place-names.mjs";

const item = (id, en) => `CustomStringTableItem "{0000000000000001}" {\n Id "${id}"\n Target_en_us "${en}"\n}\n`;
const table = [
  item("A", "Welcome to Everon"),
  item("B", "Founded at St Philippe"),
  item("C", "Prices from 1989"),
  item("D", "Welcome to town"),
  item("E", "Saint Philippe Market"),
  item("F", "Form 19890 stamped"),
].join("");
const ids = placeNameHits(table).map((h) => h.id).join(",");
if (ids !== "A,B,C,E") {
  console.log(`FAIL core-place-names-selftest: expected A,B,C,E got ${ids}`);
  process.exit(1);
}
console.log("core-place-names-selftest: OK");
