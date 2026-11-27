// tools/validation/downed-flow-order-selftest.mjs
// inputs: agent-box/proofs/downed-flow-proof.mjs
//
// The downed-flow proof judges the medic's action menu on the body from the harness `aim` line (#5155): the player's
// `GetFilteredActions`, the list the in-game menu shows and the F key acts on. Revive carries Sort Priority -10 and sits
// first (a lower number sorts first; vanilla loot is 999 and sits last). The old proof read `look`, which lists each prop's
// actions in declaration order and stops at 12 hits within 4 m, so Revive was missing near the clinic and "after Introduce"
// in the open: neither is what a player sees. The lines below are real `aim` answers from the 2026-11-27 session runs.
// red-proof: 2026-11-27, with the `revive > 0` comparison removed from judgeActionOrder this printed
// "FAIL the row it sits behind is named" and exited 1 (the `selected=` check still held the verdict); restored, it passes.
import { LIST_READS, judgeActionOrder } from "../../agent-box/proofs/downed-flow-proof.mjs";

let failed = 0;
const ok = (cond, what) => {
  if (!cond) {
    failed++;
    console.log(`FAIL ${what}`);
  }
};

// Open ground (the dock): Revive first and selected, then the give rows, Introduce last.
const OPEN = "aim selected=LF_ReviveAction@Character_Roleplay.et context=LF_ID (LF_ReviveAction@Character_Roleplay.et) offered=LF_ReviveAction@Character_Roleplay.et:1,LF_GiveItemAction@Character_Roleplay.et:0,LF_GiveCashAction@Character_Roleplay.et:0,LF_ShowIDAction@Character_Roleplay.et:0,LF_IntroduceAction@Character_Roleplay.et:1 name=#LF-Revive_Action menu=none caninteract=1 using=0 weapon=none";
// A standing player: the same context without Revive.
const LIVE = "aim selected=LF_GiveItemAction@Character_Roleplay.et context=LF_ID (LF_GiveItemAction@Character_Roleplay.et) offered=LF_GiveItemAction@Character_Roleplay.et:0,LF_GiveCashAction@Character_Roleplay.et:0,LF_ShowIDAction@Character_Roleplay.et:0,LF_IntroduceAction@Character_Roleplay.et:1 name=#LF-Interact_GiveItem menu=none caninteract=1 using=0 weapon=none";
// The clinic duty desk: the doctor stands in the view ray, so the aim is his talk row.
const DESK = "aim selected=LF_RolesAction@LF_Npc_Medic_1.et context=RoleTalk (LF_RolesAction@LF_Npc_Medic_1.et) offered=LF_RolesAction@LF_Npc_Medic_1.et:1 name=#LF-NpcTalk_Action menu=none caninteract=1 using=0 weapon=none";

ok(judgeActionOrder(OPEN).state === "pass", "the open-ground menu (Revive first and selected) passes");
const late = OPEN.replace("selected=LF_ReviveAction", "selected=LF_IntroduceAction").replace("offered=LF_ReviveAction@Character_Roleplay.et:1,LF_GiveItemAction@Character_Roleplay.et:0,LF_GiveCashAction@Character_Roleplay.et:0,LF_ShowIDAction@Character_Roleplay.et:0,LF_IntroduceAction@Character_Roleplay.et:1", "offered=LF_IntroduceAction@Character_Roleplay.et:1,LF_ReviveAction@Character_Roleplay.et:1");
ok(judgeActionOrder(late).state === "unsettled", "Revive after Introduce is not settled");
ok(/after LF_IntroduceAction/.test(judgeActionOrder(late).why), "the row it sits behind is named");
ok(judgeActionOrder(OPEN.replace("selected=LF_ReviveAction", "selected=LF_GiveItemAction")).state === "unsettled", "Revive first but not selected is not settled");
ok(judgeActionOrder(LIVE).state === "unsettled", "a body with no Revive row is not settled");
ok(judgeActionOrder(DESK).state === "unsettled" && /RoleTalk/.test(judgeActionOrder(DESK).why), "the doctor's talk row is not settled and names its context");
ok(judgeActionOrder("aim none camera=CameraBase").state === "unsettled", "no aim target is not settled");
ok(judgeActionOrder("").state === "unsettled" && judgeActionOrder(undefined).state === "unsettled", "an empty answer is not settled");
ok(LIST_READS >= 3, "the proof re-reads the menu at least three times");

if (failed) {
  console.log(`downed-flow-order-selftest: ${failed} failed`);
  process.exit(1);
}
console.log("downed-flow-order-selftest: ok");
