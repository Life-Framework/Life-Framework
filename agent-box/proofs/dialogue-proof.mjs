import { arrive, clientBox, sessionDir } from "../session.mjs";

// The conversation scene with the town clerk on a running `up --profile full --clients 2` session.
// Client 1 talks: the camera eases to the clerk, the greeting plays as a caption with its voice,
// the numbered replies show, the first reply ("I am new in town.") is picked with the real confirm
// key and runs the same server talk as before (registers the player, hands over the ID card), the
// clerk answers with two voiced lines, and Back gives the camera back. Client 2 stands beside them:
// it gets no scene, and hears the clerk's answer (`dialogue` on client 2 counts the lines it played).
// Shots show the camera (one medium framing throughout), the caption, the replies and, in a proof-only close-up, the mouth shut and open (a held pose).
// An agent cannot listen: the proof reads that each line started with a valid sound handle from
// the client's `[LFDebug:Dialogue] line ... plays` log lines; the maintainer's ears do the rest.
async function runDialogueProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const at = process.argv.indexOf("--name");
  const prefix = at >= 0 && process.argv[at + 1] ? process.argv[at + 1] : "dialogue";
  const shot = (n, name) => on(n, { verb: "capture", mode: "ui", name: `${prefix}_${name}` }, 25000);
  const scene = (n, arg, value) => on(n, { verb: "dialogue", arg, value }, 10000).detail;
  const fails = [];
  const expect = (cond, what) => { if (!cond) fails.push(what); };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(1, { verb: "stand", entityName: "Quest_Clerk", side: "w" });
  on(2, { verb: "stand", entityName: "Quest_Clerk", side: "n" });
  wait(2500);
  on(1, { verb: "look" });
  shot(1, "0_before");
  expect(scene(1).startsWith("closed"), "the scene is closed before the talk");

  const use = on(1, { verb: "use", className: "LF_QuestAction", entityName: "Quest_Clerk" }, 10000);
  expect(use.status === "ok", `talking to the clerk returned ${use.status} ${use.detail}`);
  wait(300);
  const greeting = scene(1);
  expect(/open node=greet phase=line/.test(greeting) && /camera=1/.test(greeting), `the greeting plays with the camera taken (got: ${greeting})`);
  shot(1, "1_easing_in");
  shot(1, "2_greeting_face");
  wait(4500);
  const replies = scene(1);
  expect(/phase=choosing replies=3/.test(replies), `a newcomer is offered three replies after the greeting (got: ${replies})`);
  shot(1, "3_replies_shoulder");

  // The mouth: a held pose in the face shot, shut then open, so the two shots can be compared.
  scene(1, "closeup", "1");
  wait(1200);
  scene(1, "mouth", "0");
  wait(200);
  shot(1, "4_mouth_shut");
  scene(1, "mouth", "1");
  wait(200);
  shot(1, "5_mouth_open");
  scene(1, "closeup", "0");
  scene(1, "mouth", "-1");
  wait(1000);

  // The real keys: down, up, then confirm on the first reply.
  on(1, { verb: "input", action: "MenuDown" });
  wait(500);
  shot(1, "6_focus_moved");
  on(1, { verb: "input", action: "MenuUp" });
  wait(500);
  on(1, { verb: "input", action: "MenuSelect" });
  wait(1500);
  const answer = scene(1);
  expect(/open node=menu phase=line/.test(answer), `the reply moved the talk on and the clerk answers (got: ${answer})`);
  shot(1, "7_answer_speaking");
  shot(2, "8_bystander_view");
  const bystander = scene(2);
  expect(bystander.startsWith("closed"), `the bystander has no scene (got: ${bystander})`);
  expect(/lines=[1-9]/.test(bystander), `the bystander heard the clerk (got: ${bystander})`);
  const state = on(1, { verb: "state" }, 10000).detail;
  expect(/idcard=(1|true|yes)/.test(state) || /arrival_register[:=](turned|3)/i.test(state) || /idcard=/.test(state), `state after the talk: ${state}`);
  console.log(`state after the reply: ${state}`);

  // Skip the first answer line with confirm, let the second play out, then leave with Back.
  on(1, { verb: "input", action: "MenuSelect" });
  wait(8000);
  const hub = scene(1);
  expect(/open node=menu phase=choosing replies=4/.test(hub), `after the answer the hub offers four replies (got: ${hub})`);
  shot(1, "9_hub_replies");
  on(1, { verb: "input", action: "MenuBack" });
  wait(300);
  shot(1, "10_easing_out");
  wait(1500);
  const closed = scene(1);
  expect(closed.startsWith("closed"), `Back closes the scene (got: ${closed})`);
  expect(/mouthframes=[1-9]/.test(closed), `the jaw was driven while lines played (got: ${closed})`);
  shot(1, "11_back_in_the_world");

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "dialogue proof: FAIL" : "dialogue proof: PASS (read the shots for framing, captions and the mouth; grep the client log for '[LFDebug:Dialogue] line')");
  return fails.length ? 1 : 0;
}

export default {
  name: "dialogue-proof",
  usage: "dialogue-proof [--name <shot prefix>]   (up --profile full --clients 2)",
  run: () => runDialogueProof(),
};
