// Script outside the money paths must not write a balance directly (issue #330).
// inputs: addons/**
//
// Every cash and bank change publishes LF_OnMoneyMoved from one of two choke points, which the
// money ledger records: LF_MoneyUtils (cash) and LF_BankAccount (bank). A direct write skips the
// ledger. Flags, outside Tests/:
//   - LF_BankAccount.SetBalance( anywhere but LF_BankAccount.c and LF_ATMManager.c (the save load)
//   - LF_InventoryUtils.AddAmount/RemoveAmount on PREFAB_CASH outside Feature/Money/
//
// red-proof: 2026-11-09, a scratch `account.SetBalance(5);` in Scripts/Game/Core/LF_ZZProbe.c
// printed `FAIL ...LF_ZZProbe.c:1 SetBalance( ...` and exited 1; with the file removed it prints OK.
import { execFileSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const files = execFileSync("git", ["ls-files", "-z", "--cached", "--others", "--exclude-standard", "addons"], {
  cwd: root,
  encoding: "utf8",
  maxBuffer: 1 << 28,
}).split("\0").filter((f) => f.endsWith(".c") && existsSync(join(root, f)));

const SET_BALANCE_OK = [/\/Feature\/ATM\/LF_BankAccount\.c$/, /\/Feature\/ATM\/LF_ATMManager\.c$/];
const bad = [];
for (const f of files) {
  if (f.includes("/Scripts/Game/Tests/")) continue;
  const lines = readFileSync(join(root, f), "utf8").split("\n");
  lines.forEach((raw, i) => {
    const line = raw.replace(/\/\/.*$/, "");
    if (/\bSetBalance\s*\(/.test(line) && !SET_BALANCE_OK.some((r) => r.test(f)))
      bad.push(`FAIL ${f}:${i + 1} SetBalance( writes a bank balance outside LF_BankAccount; use Deposit, Withdraw or Charge`);
    if (/LF_InventoryUtils\.(Add|Remove)Amount\s*\(.*PREFAB_CASH/.test(line) && !f.includes("/Feature/Money/"))
      bad.push(`FAIL ${f}:${i + 1} cash written outside LF_MoneyUtils; use LF_MoneyUtils.AddCash or RemoveCash`);
  });
}

if (bad.length) {
  console.log(bad.join("\n"));
  console.log(`validate-ledger-writes: ${bad.length} direct money write(s). Route them through LF_MoneyUtils / LF_BankAccount so the ledger sees them.`);
  process.exit(1);
}
console.log("validate-ledger-writes: OK");
