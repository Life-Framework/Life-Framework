import { openKit, selftestKit } from "./_island-kit.mjs";

// Second truck haul, the timber counter's board purse empty (flows2-1127): the wholesale counter must still pay and the
// toast must say why the premium buyer refused. `up --mission island --profile full --hold 20`, one client:
//   1 the harness hands out hardwood boards (`itemgive`, what a hauler carries from the saw) and the proof sells them
//     to the woodlot timber buyer press after press until its purse (32 boards, #3599) is spent and a press pays nothing
//   2 the refusal toast is the pack's "They have enough - try the wholesale warehouse at the dock" (the client's last
//     toast, and a picture of the screen)
//   3 the boards in the pockets are sold at the wholesale counter beside it: cash rises, the counter pays
// A truck haul (`truck-haul-proof`) takes 13 minutes a cycle; the purse is the same whether the boards came off a truck or
// out of `itemgive`, so this is the fast way to reach "second haul, purse empty" (`truck-haul-proof` after it sells every
// board of its first cycle at the wholesale counter).
const BOARD = "{D2A8639FA0CA09EF}Prefabs/Resources/WoodCutting/HardwoodBoard.et";
const BUYER = process.env.BUYER ?? "Fit_saint_philippe_woodlot_buyer";
const WHOLESALE = process.env.WHOLESALE ?? "Fit_saint_philippe_woodlot_wholesale";
const SELL = "LF_TraderSellAction";
const PURSE = 32;

async function run(args) {
  if (args.includes("--selftest")) {
    const f = selftestKit();
    for (const x of f) console.log(`FAIL  ${x}`);
    console.log(f.length ? "wholesale-fallback-proof selftest: FAIL" : "wholesale-fallback-proof selftest: PASS");
    return f.length ? 1 : 0;
  }
  const k = await openKit("wholesale-fallback-proof");
  const { on, wait, step, cash, carried, toast, shot } = k;
  on({ verb: "time", hour: 10 }, 15000);
  on({ verb: "resetplayer", cash: 50 }, 15000);
  wait(1500);

  // 1. Drain the premium purse: boards in, sold one press at a time at the timber buyer.
  on({ verb: "stand", entityName: BUYER });
  wait(1500);
  let premiumSold = 0;
  let refusedAt = -1;
  let lastToast = "none";
    for (let round = 0; round < 40 && refusedAt < 0; round++) {
    // A pocket holds three or four boards: a round is four gives and the presses that sell them.
    for (let i = 0; i < 4; i++) on({ verb: "itemgive", prefab: BOARD }, 10000);
    wait(800);
    for (let press = 0; press < 12; press++) {
      const before = carried("HardwoodBoard");
      if (before < 1) break;
      const r = on({ verb: "use", className: SELL, entityName: BUYER });
      wait(700);
      const sold = before - carried("HardwoodBoard");
      if (r.status === "ok" && sold > 0) {
        premiumSold += sold;
        continue;
      }
      // A press that sells nothing is the refusal: read the toast and take its picture before anything else moves.
      refusedAt = premiumSold;
      lastToast = toast();
      shot("wholesale_fallback_refused");
      break;
    }
  }
  step("1 the premium timber buyer's purse drains", refusedAt >= 0, `${premiumSold} board(s) paid before the refusal (purse cap ${PURSE}, refilling on the game clock while the proof runs)`);
  step("2 the refusal toast says the wholesale warehouse buys", /wholesale/i.test(lastToast) || /BuyerFull|BuyerNever/.test(lastToast), lastToast.slice(0, 160));

  // 3. The boards still in the pockets go to the wholesale counter beside it.
  const held = carried("HardwoodBoard");
  on({ verb: "stand", entityName: WHOLESALE });
  wait(1500);
  let wholesalePaid = 0;
  const cashBefore = cash();
  for (let press = 0; press < 16 && carried("HardwoodBoard") > 0; press++) {
    const before = cash();
    on({ verb: "use", className: SELL, entityName: WHOLESALE });
    wait(900);
    if (cash() - before <= 0) break;
    wholesalePaid += 1;
    if (wholesalePaid === 1) shot("wholesale_fallback_paid");
  }
  const earned = cash() - cashBefore;
  step("3 the wholesale counter pays for the boards the timber buyer refused", wholesalePaid >= 1 && earned > 0, `${held} board(s) in hand, ${wholesalePaid} press(es), +$${earned}`);
  return k.finish();
}

export default {
  name: "wholesale-fallback-proof",
  usage: "wholesale-fallback-proof [--selftest]   (on `up --mission island --profile full`: drains the timber buyer's board purse, then the wholesale counter pays)",
  run: (ctx) => run(ctx?.args ?? []),
};
