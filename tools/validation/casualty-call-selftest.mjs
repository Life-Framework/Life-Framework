// tools/validation/casualty-call-selftest.mjs
// inputs: agent-box/**
//
// The casualty-call proof's pure parts (#4830, #4853): the `casualty roll [kind]` verb reaches the game as one line, `stand Casualty`
// is a valid stand, the roll reply and the server's casualty line parse (a reply or line that drifts breaks the played medic hour),
// and judgeCasualty fails each missing step: no take, no treat, no board close, no map mark close, no bonus, no shift pay.
// red-proof: 2026-11-24, with the "on the medic board closed" needle dropped from judgeCasualty this printed
// "FAIL a treat with no board close is judged: ..." and exited 1; restored, it passes.
import { buildLine } from "../../agent-box/harness.mjs";
import { findLine, judgeCasualty, parseCasualty, parseRollReply } from "../../agent-box/proofs/casualty-call.mjs";

let failed = 0;
const ok = (cond, what) => {
  if (!cond) {
    failed++;
    console.log(`FAIL ${what}`);
  }
};

ok(buildLine(7, { verb: "casualty", words: ["roll"] }) === "7 casualty roll", "casualty roll is one line");
ok(buildLine(8, { verb: "casualty", words: ["roll", "Dock"] }) === "8 casualty roll Dock", "casualty roll Dock names the kind");
ok(buildLine(9, { verb: "stand", entityName: "Casualty" }) === "9 stand Casualty", "stand Casualty is a stand");

const casualtyLine = "[LFDebug:CasualtyCall] casualty casualty:1 '#LF-CasualtyCall_Dock' at Dock (<4426.74,1.59,11094.1>); 1 open; expires in 600 s (board 900 s, bleed-out matches)";
const parsed = parseCasualty(["noise", casualtyLine]);
ok(parsed && parsed.uid === "casualty:1" && parsed.kind === "Dock" && parsed.open === 1 && parsed.at[0] === 4426.74, "the server's casualty line parses");

const reply = parseRollReply("casualty:2 '#LF-CasualtyCall_Dock' at <4426.7,1.6,11094.1> expires_in=599");
ok(reply && reply.uid === "casualty:2" && reply.expiresIn === 599 && reply.at[2] === 11094.1, "the roll reply parses");
ok(parseRollReply("error no_casualty_rolled") === null, "a refusal is not a roll reply");

const good = [
  casualtyLine,
  "[LFDebug:ServiceBoard] call #1 on the medic board taken by 1 (1/1)",
  "[LFDebug:CasualtyTreat] casualty casualty:1 treated by medic abc; call closed, revive bonus and MEDIC xp booked",
  "[LFDebug:ServiceBoard] call #1 on the medic board closed (casualty:1)",
  "[LFDebug:ServiceCalls] medic call 'grid 044-110' closed: map mark dispatch:1 removed",
  "[LFDebug:DutyBonus] bonus +$10 (REVIVE) for abc: $10 of $60 this hour, $10 pending",
  "[LFDebug:DutyPay] duty bonus +10 cash",
  "[LFDebug:DutyPay] shift pay +7 cash",
];
ok(judgeCasualty(good).length === 0, `a full hour passes: ${judgeCasualty(good).join("; ")}`);
ok(findLine(good, 0, "[LFDebug:DutyBonus]", "REVIVE") === 5, "findLine finds the bonus line");

const without = (needle) => good.filter((l) => !l.includes(needle));
for (const [needle, what] of [
  ["taken by", "take"],
  ["treated by medic", "treat"],
  ["on the medic board closed", "board close"],
  ["closed: map mark", "map mark close"],
  ["bonus +$10", "bonus"],
  ["duty bonus +", "payslip bonus"],
  ["shift pay +", "shift pay"],
]) {
  const fails = judgeCasualty(without(needle));
  ok(fails.length > 0, `a treat with no ${what} is judged: ${JSON.stringify(fails)}`);
}
ok(judgeCasualty(without("treated by medic"), { treat: false }).every((f) => !f.startsWith("treat")), "--skip-treat does not expect a treat");
ok(judgeCasualty([]).length === 1, "no casualty at all is one failure");

console.log(failed ? `SUMMARY casualty-call-selftest fail ${failed}` : "SUMMARY casualty-call-selftest ok");
process.exit(failed ? 1 : 0);
