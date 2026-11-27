// tools/validation/downed-flow-order-selftest.mjs
// inputs: agent-box/proofs/downed-flow-proof.mjs
//
// The downed-flow proof judges the medic's action order on a list read with the body in front of them (#5155). One run
// flaked "Revive is listed after Introduce" while the medic still faced the duty desk. `judgeActionOrder` calls a list
// with no Revive, or with Revive after Introduce, "unsettled" (the proof stands, looks down and reads it again, up to
// LIST_READS times); only a list with Revive first, or Revive alone, passes.
// red-proof: 2026-11-27, with the Revive-before-Introduce comparison removed from judgeActionOrder this printed
// "FAIL Revive after Introduce is not settled" and exited 1; restored, it passes.
import { LIST_READS, judgeActionOrder } from "../../agent-box/proofs/downed-flow-proof.mjs";

let failed = 0;
const ok = (cond, what) => {
  if (!cond) {
    failed++;
    console.log(`FAIL ${what}`);
  }
};

ok(judgeActionOrder("LF_ReviveAction on Player2, LF_IntroduceAction on Player2").state === "pass", "Revive then Introduce passes");
ok(judgeActionOrder("LF_ReviveAction on Player2").state === "pass", "Revive alone passes");
ok(judgeActionOrder("Revive, Introduce").state === "pass", "plain-text Revive then Introduce passes");
ok(judgeActionOrder("LF_IntroduceAction on Player2, LF_ReviveAction on Player2").state === "unsettled", "Revive after Introduce is not settled");
ok(judgeActionOrder("LF_DutyAction on Fit_saint_philippe_clinic_lf_medic_duty_desk").state === "unsettled", "the duty desk's list is not settled");
ok(/duty desk/.test(judgeActionOrder("LF_DutyAction on desk").why), "the desk's list says so");
ok(judgeActionOrder("").state === "unsettled" && judgeActionOrder(undefined).state === "unsettled", "an empty list is not settled");
ok(LIST_READS >= 3, "the proof re-reads the list at least three times");

if (failed) {
  console.log(`downed-flow-order-selftest: ${failed} failed`);
  process.exit(1);
}
console.log("downed-flow-order-selftest: ok");
