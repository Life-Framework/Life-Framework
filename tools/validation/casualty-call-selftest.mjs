// tools/validation/casualty-call-selftest.mjs
// inputs: agent-box/**
//
// The casualty-call proof's pure parts (#4830, #4853): the `casualty roll [kind]` verb reaches the game as one line, `stand Casualty`
// is a valid stand, the roll reply and the server's casualty line parse (a reply or line that drifts breaks the played medic hour),
// and judgeCasualty fails each missing step: no take, no treat, no board close, no map mark close, no bonus, no shift pay.
// red-proof: 2026-11-24, with the "on the medic board closed" needle dropped from judgeCasualty this printed
// "FAIL a treat with no board close is judged: ..." and exited 1; restored, it passes.
import { buildLine } from "../../agent-box/harness.mjs";
import { bonusOutcome, findLine, judgeBed, judgeCasualty, judgeDraw, judgeDressed, parseCasualty, parseRollReply } from "../../agent-box/proofs/casualty-call.mjs";

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
// A re-run in the same game hour meets the hourly cap: the bonus is capped, not booked, and the payslip has no bonus line.
const capped = good.filter((l) => !l.includes("bonus +$10") && !l.includes("duty bonus +")).concat("[LFDebug:DutyBonus] bonus capped for abc: $40 of $40 this hour already earned");
ok(judgeCasualty(capped).length === 0, `a capped bonus passes (the hourly cap is working): ${judgeCasualty(capped).join("; ")}`);
ok(bonusOutcome(good, 0, "REVIVE") === "booked" && bonusOutcome(capped, 0, "REVIVE") === "capped", "bonusOutcome tells booked from capped");
ok(bonusOutcome(without("bonus +$10"), 0, "REVIVE") === "none", "no bonus line at all is none");
ok(judgeCasualty(without("bonus +$10")).length > 0, "a treat with no bonus decision at all is judged");
ok(judgeCasualty(without("treated by medic"), { treat: false }).every((f) => !f.startsWith("treat")), "--skip-treat does not expect a treat");
ok(judgeCasualty([]).length === 1, "no casualty at all is one failure");

// #4863: dressed casualties on clear ground.
const dressedLine = "[LFDebug:CasualtyCall] casualty dressed in 4 piece(s) of {F2C48A895452DDCF}Configs/NpcOutfits/LF_Outfit_DockDocker1.conf";
ok(judgeDressed([dressedLine, dressedLine], 2).length === 0, "two dressed casualties pass");
ok(judgeDressed([dressedLine], 2).length === 1, "a casualty with no dressed line is judged");
ok(judgeDressed([dressedLine.replace("in 4", "in 1")], 1).length === 1, "a casualty with one garment is judged");
ok(judgeDressed([dressedLine, "[LFDebug:CasualtyCall] no clear ground near the Dock place at <1,2,3> (under a roof)"], 1).length === 1, "a roll with no clear ground is judged");

// #4849: a bed delivery closes the call, the board line and the map mark.
const bedLines = [
  "[LFDebug:Medic] player npc put in a hospital bed by medic abc",
  "[LFDebug:ServiceBoard] call #2 on the medic board closed (casualty:2)",
  "[LFDebug:ServiceCalls] medic call 'grid 044-110' closed: map mark dispatch:2 removed",
  "[LFDebug:CasualtyTreat] casualty casualty:2 put in a bed; call closed",
];
ok(judgeBed(bedLines, "casualty:2").length === 0, `a bed delivery that closes everything passes: ${judgeBed(bedLines, "casualty:2").join("; ")}`);
ok(judgeBed(bedLines.filter((l) => !l.includes("put in a bed; call closed")), "casualty:2").length === 1, "a delivery that never closed the call is judged");
ok(judgeBed(bedLines.slice(1), "casualty:2").length === 1, "no bed delivery at all is judged");
ok(judgeBed(bedLines.filter((l) => !l.includes("on the medic board closed")), "casualty:2").length === 1, "a delivery that left the board line is judged");
ok(judgeBed(bedLines.filter((l) => !l.includes("closed: map mark")), "casualty:2").length === 1, "a delivery that left the map mark is judged");

const drewLine = "16:37:12.615 SCRIPT : [LFDebug:MedicCarPool] uid (rank 0) drew {CD9E4E997EA1DFEF}Prefabs/Vehicles/Medic/LF_UAZ452_AmbulanceFleet.et into the bay at <4422.63,10.0254,10766.4>";
const refusedLine = "16:35:00.956 SCRIPT (W): [LFDebug:MedicCarPool] draw refused: a vehicle stands in the bay at <4422.63,10.73,10766.4>: S105_beige.et (id 763) at <4425,10.5,10766>, 2.4 m from the bay centre";
ok(judgeDraw([drewLine]).length === 0, "a logged draw passes");
ok(judgeDraw([]).length === 1, "no draw line at all is judged");
ok(judgeDraw([refusedLine]).length === 1 && judgeDraw([refusedLine])[0].includes("S105_beige.et"), "a refused draw is judged and names the blocker");

console.log(failed ? `SUMMARY casualty-call-selftest fail ${failed}` : "SUMMARY casualty-call-selftest ok");
process.exit(failed ? 1 : 0);
