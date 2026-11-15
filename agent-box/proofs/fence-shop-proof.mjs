import { arrive, clientBox, sessionDir } from "../session.mjs";

// The fence as a shopkeeper (#1854), driven the way a player does it, on the pack mission so the
// fence stock overlay loads:
//   node agent-box/session.mjs up --profile full --mission playtest
//   node agent-box/session.mjs fence-shop-proof
//   node agent-box/session.mjs down
// Stands 2 m from Vince: the F key offers Talk and Browse goods. Talks, asks what he does, picks
// "Show me what you've got", and the price list opens. Buys a lockpick ($250), then sells car parts
// from the buyback lines. Confirm `[LFDebug:Shop] bought item=...LF_Lockpick` and
// `sold item=...LF_CarParts ... stock=fence` in the server log.
const NPC = "FenceNpc";
const TALK = "LF_RolesAction";
const SHOP = "LF_ShopAction";
const PARTS = "{7A94C1E05D3B2F68}Prefabs/Items/Resources/LF_CarParts.et";

async function runFenceShopProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 240)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const expect = (cond, what) => { if (!cond) fails.push(what); };
  const shot = (name) => on(1, { verb: "capture", mode: "ui", name }, 25000);

  arrive(on, 1, wait);
  on(1, { verb: "input", action: "MenuBack" });
  wait(4000);
  on(1, { verb: "resetplayer", cash: 1000 }, 10000);
  wait(1500);
  expect(on(1, { verb: "itemgive", prefab: PARTS }, 10000).status === "ok", "car parts handed to the player");

  on(1, { verb: "stand", entityName: NPC, side: "n2" });
  wait(2500);
  const look = on(1, { verb: "look" });
  expect(look.detail.includes(TALK), `the F key offers Talk 2 m from the fence: ${look.detail.slice(0, 200)}`);
  expect(look.detail.includes(SHOP), `the F key offers Browse goods 2 m from the fence: ${look.detail.slice(0, 200)}`);
  shot("fence_talk_or_browse");

  // Talk: greeting, "what do you do", then the shop reply.
  expect(on(1, { verb: "use", className: TALK, entityName: NPC }, 10000).status === "ok", "talk to the fence");
  wait(2500);
  const scene = (arg, value) => on(1, { verb: "dialogue", arg, value }, 10000).detail;
  expect(/open node=greet/.test(scene()), "the fence's greeting opens");
  shot("fence_talk_greeting");
  wait(5000);
  scene("pick", "0");
  // The answer line plays first; the replies show once it ends.
  let what = "";
  for (let i = 0; i < 6; i++) {
    wait(2500);
    what = scene();
    if (/phase=choosing/.test(what)) break;
  }
  expect(/open node=what/.test(what) && /replies=3/.test(what), `"what do you do" offers three replies (got: ${what})`);
  shot("fence_talk_show_goods_reply");
  scene("pick", "0");
  wait(5000);
  expect(scene().startsWith("closed"), "the show-goods reply ends the talk");

  // The price list opened by the reply.
  const list = on(1, { verb: "ui", mode: "text" }, 10000).detail;
  expect(/Item0Row_Action|Lockpick/i.test(list), `the price list opened from the talk (got: ${list.slice(0, 200)})`);
  shot("fence_price_list");

  // Buy a lockpick: the first line.
  on(1, { verb: "click", widget: "Item0Row_Action" });
  wait(600);
  on(1, { verb: "click", widget: "PayButton" });
  wait(2500);
  const bought = on(1, { verb: "ui", mode: "text" }, 10000).detail;
  expect(/750/.test(bought), `$250 taken from $1,000 (got: ${bought.slice(0, 240)})`);
  shot("fence_bought_lockpick");

  // Sell car parts: the buyback line after the five tools.
  on(1, { verb: "click", widget: "Item5Row_Action" });
  wait(600);
  shot("fence_sell_slip");
  on(1, { verb: "click", widget: "SellButton" });
  wait(2500);
  const sold = on(1, { verb: "ui", mode: "text" }, 10000).detail;
  expect(/850/.test(sold), `$100 paid for the car parts (got: ${sold.slice(0, 240)})`);
  shot("fence_sold_car_parts");
  on(1, { verb: "click", widget: "CloseButton" });

  // The browse action opens the same list directly.
  wait(1500);
  expect(on(1, { verb: "use", className: SHOP, entityName: NPC }, 10000).status === "ok", "Browse goods opens the list");
  wait(1500);
  shot("fence_browse_direct");
  on(1, { verb: "click", widget: "CloseButton" });

  if (fails.length) {
    console.log("fence-shop-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("fence-shop-proof: OK");
  return 0;
}

export default { name: "fence-shop-proof", usage: "fence-shop-proof", run: () => runFenceShopProof() };
