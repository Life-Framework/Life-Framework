# Island first hour: the 15-minute smoke

The weekend promise on real Everon (Saint Philippe, `LF_EveronIsland.conf`), walked as a new player.
Two forms of the same route:

- **Scripted**: `agent-box/flows/island-first-hour.txt` (#2935), the Friday go/no-go gate on main.
- **Manual**: this page, the post-publish smoke on the 0grind alpha (a fresh character, about 15 minutes).

## Run the scripted flow

```powershell
node agent-box/session.mjs up --mission island --clients 1 --roster 1 --profile full --wait
node agent-box/harness.mjs --client 1 flow island-first-hour
node agent-box/session.mjs down
```

Use a fresh client profile (the quiz and form R-89 must be the first screens; step 1 checks the starting
$100 and $1000). The flow judges itself: each `step` reads the player's state when it opens, and its
`expect` lines (`cash -250`, `job PARCEL`, `duty police`, `item WaterBottle`, the list is in
`agent-box/flow-asserts.mjs`) check what moved. A failed line or assert fails the step, and a failed step
does not stop the run. It ends with one `step PASS|FAIL` row per step and a tally line such as
`flow island-first-hour: 2 pass (1, 10), 10 fail of 12 steps`, and exits 1 when any step failed.
A `use` whose named entity is missing fails as `entity_not_found` or `wrong_target:<what it would have hit>`.
When a slot card lands, check its entity names against `Place_saint-philippe_<slot>.layer`, fix the flow
line if a kit row is named differently, and drop the `pending #N` from that step.

## The route

| # | Do | Observe | Slot card |
|---|---|---|---|
| 1 | Join, pass the rules quiz (5 questions), sign form R-89 | A new character wakes in a hostel first-spawn bed (about 4474, 10762, inside the 25 m hostel safe circle); a later or bedless spawn lands on the town hall forecourt (inside the 52 m square circle). $100 cash and $1000 in the bank; walk to the square (#3350 follow-up) | on main |
| 2 | Talk to the clerk at the town hall; read the rules sign | The clerk greets you and points to work; the rules sign opens | pending #2932 |
| 3 | At the parcel kiosk: Take courier work (parcel only), Pick up parcel; carry it to a drop: a mailbox 300 m or more away (the dock metal shed north of the square, or the wooden house east), wait out the walk, Complete the run | A toast pays the run (about $40); the job shows Courier | draft #2325 and #2959 |
| 4 | Open your status | Cash went up by the pay; job XP or level moved | follows step 3 |
| 5 | Bank teller: deposit the pay, then withdraw $300 (permit and driving licence) | Bank and cash move by the amounts you typed | pending #2932 |
| 6 | Clothing shop: buy a shirt | Cash down by the price; the shirt is in your inventory | pending #2315 |
| 6b | Gun counter: order the Firearm permit ($250), pay cash | The permit is yours; the PM pistol is no longer "Not without a Firearm permit" | on main (needs $250 cash, so step 5 first) |
| 6c | Town hall licence desk: buy the driving licence ($100, cash) | Cash down $100; the licence is yours at once | on main (#3140) |
| 6d | Car lot: buy the cheapest car (the lot refuses you without the driving licence, #3151) | Bank or cash down; the car is yours and parked at the lot | pending #2933 |
| 7 | General store: buy water | Cash down; water in the inventory. The till is there to rob (police get an alert) | pending #2314 |
| 8 | Police station desk: clock in, clock out (whitelisted police only) | Duty on, police kit and radio; duty off | pending #2319 |
| 9 | Clinic desk: clock in, clock out (medic roster) | Duty on as medic; duty off | pending #2320 |
| 10 | Town board on the square: take a card | The card reads; its "Where" names a real place in town, not "Ask around" | board on main; targets come with the cards above |

Note anything that blocks a new player in the Discord `#dev` channel with the step number.

## Flow status

Self-judged run on `ws/first-hour-asserts` over main `14ffae11` (2026-11-18, one client, `--mission island`): `2 pass (1, 10), 10 fail of 12 steps`. Step 7 now fails as `wrong_target:Fit_saint_philippe_gun_counter_lf_gun_counter` instead of opening the gun counter. Per step:

| # | Step | Result |
|---|---|---|
| 1 | Quiz, form R-89, spawn, walk to the square | **pass**: spawned at the south road with cash 100, bank 1000; stood at the arrival place |
| 2 | Clerk and rules sign | pending #2932 (no clerk or rules sign in the arrival layer) |
| 3 | Parcel job | draft #2325 (parcel kiosk) |
| 4 | Paid and progression | draft #2959 (parcel drops); flow waits out the timed run |
| 5 | Bank teller | pending #2932 (bank layer has the vault and counter marker, no teller) |
| 6 | Shirt | pending #2315 (clothing layer has props, no shop) |
| 6b | Firearm permit | **works, unaffordable**: the counter opens and lists the permit at $250; a newcomer's $100 is refused. Passes once step 5 can withdraw |
| 6c | Driving licence | not yet run (added by #3151) |
| 6d | Car | pending #2933 (no car-lot slot) |
| 7 | Groceries | pending #2314 |
| 8 | Police duty | pending #2319 |
| 9 | Medic duty | pending #2320 |
| 10 | Town board | **pass**: the card opens ("Couriers wanted"); its "Where" says "Ask around" until the slots carry their signs |
