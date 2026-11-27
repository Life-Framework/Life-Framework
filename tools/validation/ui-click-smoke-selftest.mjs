#!/usr/bin/env node
// inputs: agent-box/proofs/ui-click-smoke.mjs
// Offline checks of the pure helpers of the `ui-click-smoke` proof: which widget a player would press first, which list
// rows have no button (the #5099 failure: "What to do next" rows with no Read button), and how a click answer reads.
// red-proof: 2026-11-27, observed: rowsWithoutButton made to return [] (the pre-#5099 state, no _Action widgets) -> FAIL rowsWithoutButton: three rows and no buttons are three dead rows
import { clickVerdict, clickables, firstActionable, readState, rowsWithoutButton } from "../../agent-box/proofs/ui-click-smoke.mjs";

const fails = [];
const ok = (cond, what) => {
  if (!cond) fails.push(what);
};

ok(clickables("none").length === 0 && clickables("").length === 0, "clickables: none is an empty screen");
ok(clickables("A,B_Action, C").join("|") === "A|B_Action|C", "clickables: splits and trims");

ok(firstActionable(["PlayerMenuCloseButton", "Row0_Action", "Row1_Action"]) === "Row0_Action", "firstActionable: a row's own button comes first");
ok(firstActionable(["LotClose", "Buy"]) === "Buy", "firstActionable: skips a way out");
ok(firstActionable(["LotClose", "Keys", "WelcomeGuideDismissButton"]) === "", "firstActionable: only ways out is no button");
ok(firstActionable([]) === "", "firstActionable: an empty screen has none");

ok(rowsWithoutButton(3, ["PlayerMenuNext_0_Action", "PlayerMenuNext_1_Action", "PlayerMenuNext_2_Action"], "PlayerMenuNext_").length === 0, "rowsWithoutButton: every row has its button");
ok(rowsWithoutButton(3, ["PlayerMenuNext_0_Action", "PlayerMenuNext_2_Action"], "PlayerMenuNext_").join() === "1", "rowsWithoutButton: names the row with none");
ok(rowsWithoutButton(3, ["PlayerMenuNextGo", "PlayerMenuCloseButton"], "PlayerMenuNext_").join() === "0,1,2", "rowsWithoutButton: three rows and no buttons are three dead rows");
ok(rowsWithoutButton(2, ["Other_0_Action", "Other_1_Action"], "PlayerMenuNext_").join() === "0,1", "rowsWithoutButton: another list's buttons do not count");

ok(clickVerdict("ok", "PlayerMenuNext_0_Action") === "ok", "clickVerdict: ok");
ok(clickVerdict("error", "widget_not_found PlayerMenuNext_2_Action") === "dead", "clickVerdict: widget_not_found is a dead button");
ok(clickVerdict("error", "click_unhandled") === "other", "clickVerdict: click_unhandled is another failure");
ok(clickVerdict("timeout", "") === "other", "clickVerdict: a timeout is another failure");

const state = readState("rows=3 picked=0 ids=mining:1,lumber:1");
ok(state.rows === 3 && state.picked === 0 && state.ids === "mining:1,lumber:1", "readState: numbers and the id list");
ok(readState("open=1 tab=1 data=1 skills=4 quests=2 licences=0").quests === 2, "readState: playermenu state");
ok(readState("").rows === undefined, "readState: empty answer");

for (const f of fails) console.log(`FAIL ${f}`);
console.log(`SUMMARY ui-click-smoke-selftest ${fails.length ? "fail" : "ok"} ${fails.length} failed`);
process.exit(fails.length ? 1 : 0);
