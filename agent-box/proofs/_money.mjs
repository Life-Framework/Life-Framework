// agent-box/proofs/_money.mjs - read a money amount out of game text, the one way (#5163).
//
// LF_MoneyFormat.FormatMoney writes "$1,040" and "-$1,040" (a thousands comma, the sign before the "$"); a raw int in
// a debug line writes "$1040". A proof regex of the form `\$(\d+)` reads "$1,040" as 1, so a paid amount of 1040 passed
// as a "paid $1" mismatch (RC dry run 6, export-crane-proof). Every proof, flow and harness parser that reads an amount
// uses this module, never its own `\d+`:
//
//   new RegExp(String.raw`export order handed in by \S+: (${MONEY})`)    capture the whole "$1,040" / "-$1,040"
//   new RegExp(String.raw`fine=\$(${AMOUNT})`)                            capture only the digits after a literal "$"
//   parseMoney("-$1,040") === -1040      parseMoney(m[1])                 text -> number (NaN when it holds none)
//   firstMoney(text)                                                      the first amount on a screen text, or null
//   allMoney(text)                                                        every amount on it, in order
//
// `tools/validation/proof-money-selftest.mjs` proves the parts and fails a proof file that still holds a bare
// `\$(\d+)`; a deliberate raw-digit read is marked `// lf-money-ok: <why>` on its line.

/** Digits with optional thousands commas: "1040", "1,040", "12,345,678". Not a capture group. */
export const AMOUNT = String.raw`\d+(?:,\d{3})*`;

/** One amount as the game prints it: "$1,040", "-$1,040", "+$1,040", "$ 1040". Not a capture group. */
export const MONEY = String.raw`[+-]?\$ ?${AMOUNT}`;

/** "$1,040" -> 1040, "-$1,040" -> -1040, "+$5" -> 5, "1,040" -> 1040, "1040" -> 1040. NaN when there is no amount. */
export function parseMoney(text) {
  const m = new RegExp(String.raw`([+-]?)\$? ?(${AMOUNT})`).exec(String(text ?? ""));
  if (!m) return NaN;
  const n = Number(m[2].replace(/,/g, ""));
  return m[1] === "-" ? -n : n;
}

/** Every "$" amount in a text, as numbers, in reading order. */
export function allMoney(text) {
  return [...String(text ?? "").matchAll(new RegExp(MONEY, "g"))].map((m) => parseMoney(m[0]));
}

/** The first "$" amount in a text, or null. */
export function firstMoney(text) {
  const all = allMoney(text);
  return all.length ? all[0] : null;
}
