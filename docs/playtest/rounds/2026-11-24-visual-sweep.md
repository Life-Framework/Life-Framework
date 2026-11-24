# Visual sweep, 0.3.0 (2026-11-24)

The first hours of a new player on Everon, at eye height, midday, in one `--mission island --profile full` session
(`session down` straight after), graded against the bar in `docs/style-guide.md`. Refs #4180. 0.3.0 is in feature
freeze, so this is polish only.

Method: `agent-box/harness.mjs shot world <name> x y z ox oy oz` poses the character at eye height and faces the aim
point (default clear sky, noon). The aim points and stand points come from the placed slot layers
(`Place_*.layer`), not typed by hand. 170 pictures in the first session, then a short second one to read the supermarket
log. Four pictures sit on each contact sheet in `website/public/docs-images/4180/visual-sweep/sheet-NNN.jpg` (label in
the lower left is the picture number and name; the in-game "town clerk" waypoint tag and the hint card are the game's HUD).

Grade key: **M** meets the bar, **B** below it (a slice card is named), **R** the vantage was unusable (wall, hedge, trunk,
camera under the map) and a later retake of the same place carries the grade. 85 M, 23 B, 62 R.

Already being fixed elsewhere and not redone: north outpost move, mine shift board, flag wall, blank signs (so the
back of a sign reads blank), pharmacy shelves, fish stall, smelter, mine workbench. The smelter and the Highstone and
East Rise sign backs appear below because they are on the sweep list; they are graded as seen.

## Places

| Place | Grade | One line | Card |
|---|---|---|---|
| Saint-Philippe square | meets | open cobbles, church, houses, kerb rails, no floating props | |
| Main street | meets | road, garages, houses, poles; first take fell under unstreamed terrain, retaken | |
| Town hall interior | meets | hall with stair, benches, chandeliers, notice board | |
| Clerk | meets | behind the glass hatch, facing the hall | |
| Garage | below | shop room is a bare washroom, livery bay a plain green bench | #4904 |
| Car lot | meets | vans parked on the slab; sign and mechanic not seen well from the slope | |
| Orchard and juice press | meets | trees in rows, press, desk, crates, hay | |
| Market | meets | stocked crates, price boards, vendors, awning | |
| Cafe | meets | laid tables, bar, barkeeper | |
| Chotain bar | below | no pub sign seen, kitchen look, bare shelves | #4902 |
| Levie bar | meets | counter, stools, bottles, curtains (sparse) | #4902 note |
| Bank | meets | red BANK board, teller hall with registers, vault corridor | |
| Supermarket | below | gondolas empty, goods only on back wall units; fascia text clipped by the canopy | #4901 |
| General store | meets | stocked shelves, posters, till, safe | |
| Butcher | below | dark counter, nothing on show | #4903 |
| Bakery | below | dark, one rack, three loaves | #4903 |
| Hardware store | meets | yard shed with counter, planks, clerk | |
| Clinic | below | sign is small beside its neighbours; interior furnished | #4905 |
| Police hall | meets | officer, boards, desk, booking desk, cell | |
| Dock and harbor | meets | crane, containers, smokehouse, harbour office plate | |
| Coalman's Brow | below | counter and desk sunk in grass, upright rust plates | #4906 |
| Highstone | meets | readable sign, cart, timber, fence; sparse | |
| East Rise | meets | readable sign, boulders; coal heap is grey gravel | #4906 note |
| Compound smelter | meets | furnace, wheelbarrow, truck, silo (fix in flight) | |
| Power plant yard | below | the coal buyer is a yellow box on dirt | #4907 |
| Sawmill and woodlot | meets | saw lines, racks, planks, sign | |
| Truck yard | meets | truck row, examiner, sign, check gate | |
| Hostel | meets | fascia, furnished rooms, ground-floor hall | |

Not seen well, so not graded as below: the car lot sign and mechanic, the clinic beds, the butcher meat display up close
(its dried meat is dark brown by design), the dock warehouse and wholesale interiors.

## Findings and cards

- **Supermarket gondolas empty** (#4901). The server logs `store everon_sp_supermarket stocked` with 150 items, but they
  stand only on the host's dark wall units (`ShelfUniversal_01_wide`); the sloped `ShelfShop_01` gondolas a player
  faces at the door carry nothing. `sheet-044`, `sheet-048`, `sheet-135`.
- **Chotain bar** (#4902). `sheet-028` to `sheet-032`.
- **Butcher, bakery** (#4903). `sheet-052`, `sheet-056`, `sheet-159`.
- **Garage shop room and livery bay** (#4904). `sheet-012`, `sheet-159`.
- **Clinic sign** (#4905). `sheet-064`.
- **Coalman's Brow in grass** (#4906). `sheet-080` to `sheet-084`, `sheet-147` to `sheet-149`.
- **Power plant coal buyer** (#4907). `sheet-096`, `sheet-151`, `sheet-155`.

## Fixes shipped by this sweep

None in a `town-draft-<place>-<slot>` worktree. Every below-grade place needs a kit change plus an island `look`
before and after (about 25 minutes of queue each), and none of the seven is a one-row nudge: the supermarket needs a
second shelf fixture in `tools/town/shelf-stock.mjs`, the bars, butcher, bakery and garage need goods, and the mine and
coal yard need cleared ground. They are cards, one slot each, so they run in parallel.

## Every picture

| # | Shot | Grade | Note | Sheet |
|---|---|---|---|---|
| 0 | square-from-sw | R | hedge and bus shelter in the way | sheet-000 |
| 1 | square-from-north | R | landed inside the church; pews and paintings are fine | sheet-000 |
| 2 | square-bench-chess | M | church, houses, fence, parasol, cobbles: nothing off | sheet-000 |
| 3 | main-street-west | R | camera under the map: terrain had not streamed, see 163 | sheet-000 |
| 4 | main-street-east | R | wall corner | sheet-004 |
| 5 | townhall-exterior | M | town hall front: sign board, forecourt, lamp posts, a resident on the steps | sheet-004 |
| 6 | townhall-clerk-wide | R | window only | sheet-004 |
| 7 | townhall-clerk-close | M | clerk's glass hatch, desk lamp, plant; clerk seen in 126 | sheet-004 |
| 8 | townhall-labour-counter | M | hall through the glass doors: stair, benches, plant, chandeliers | sheet-008 |
| 9 | townhall-rules-sign | M | TOWN RULES plate readable by the door, bench, planter | sheet-008 |
| 10 | garage-exterior | M | GARAGE fascia partly behind a bush ("AGE" at this angle); shed on its apron | sheet-008 |
| 11 | garage-counter | R | looking through the shop window | sheet-008 |
| 12 | garage-livery-bay | M | livery bay is a plain green bench on asphalt: plain but not wrong (see V4 #4904) | sheet-012 |
| 13 | garage-shelf | B | shop room is a tiled washroom: sink, locker, one empty shelf unit (#4904) | sheet-012 |
| 14 | carlot-wide | R | bush, wall or stairwell in the way; the lot is graded from 129 to 131 | sheet-012 |
| 15 | carlot-uaz469 | R | bush, wall or stairwell in the way; the lot is graded from 129 to 131 | sheet-012 |
| 16 | carlot-uaz452 | R | bush, wall or stairwell in the way; the lot is graded from 129 to 131 | sheet-016 |
| 17 | carlot-sign-mechanic | R | bush, wall or stairwell in the way; the lot is graded from 129 to 131 | sheet-016 |
| 18 | orchard-barn | M | fruit trees in rows, barn behind, grass trimmed along the path | sheet-016 |
| 19 | orchard-rows | M | fruit trees in rows, barn behind, grass trimmed along the path | sheet-016 |
| 20 | orchard-stall | M | juice stall with the press, hay bale, crates, bucket, desk with its box: one cluster, nothing floating | sheet-020 |
| 21 | orchard-juicer-close | M | juice stall with the press, hay bale, crates, bucket, desk with its box: one cluster, nothing floating | sheet-020 |
| 22 | orchard-desk | M | juice stall with the press, hay bale, crates, bucket, desk with its box: one cluster, nothing floating | sheet-020 |
| 23 | market-stalls-front | M | stalls with stock crates, price boards, awning, vendors at their tables, blue rail along the kerb | sheet-020 |
| 24 | market-stalls-left | M | stalls with stock crates, price boards, awning, vendors at their tables, blue rail along the kerb | sheet-024 |
| 25 | market-stalls-right | M | stalls with stock crates, price boards, awning, vendors at their tables, blue rail along the kerb | sheet-024 |
| 26 | market-trader-close | M | stalls with stock crates, price boards, awning, vendors at their tables, blue rail along the kerb | sheet-024 |
| 27 | cafe-exterior | R | wrong side, a gate in a fence | sheet-024 |
| 28 | cafe-till | R | tiled kitchen and a door: the vantage is behind the bar | sheet-028 |
| 29 | cafe-room-cross | R | tiled kitchen and a door: the vantage is behind the bar | sheet-028 |
| 30 | cafe-room-back | M | cafe hall: laid tables with cloths, chairs, bar and barkeeper, lit by day | sheet-028 |
| 31 | chotain-bar-front | B | Chotain bar: plain yellow counter, one bottle, wallpapered kitchenette (#4902) | sheet-028 |
| 32 | chotain-bar-shelf | B | shelf units behind the bar empty; the room reads as a kitchen (#4902) | sheet-032 |
| 33 | chotain-bar-wide | R | gable wall | sheet-032 |
| 34 | chotain-exterior-a | B | Chotain bar from the road: a farmhouse with a garage door, no pub sign on the sides shot (#4902) | sheet-032 |
| 35 | chotain-exterior-b | R | garage door and a tree | sheet-032 |
| 36 | levie-bar-front | M | Levie bar: counter, four stools, bottles on the shelf, curtains. Sparse, not wrong | sheet-036 |
| 37 | levie-bar-shelf | M | Levie bar shelf: two units with a few bottles, barkeeper in place | sheet-036 |
| 38 | levie-bar-wide | R | wall, spruce, wall | sheet-036 |
| 39 | levie-exterior-a | R | wall, spruce, wall | sheet-036 |
| 40 | levie-exterior-b | R | wall, spruce, wall | sheet-040 |
| 41 | bank-exterior | R | water tower in front; bank graded from 132 and 133 | sheet-040 |
| 42 | bank-teller | M | bank counter: glass screen, cash registers, teller; dark but legible | sheet-040 |
| 43 | bank-vault | M | vault corridor: steel door, wash basin, bin | sheet-040 |
| 44 | bank-hall-wide | R | outside wall | sheet-044 |
| 45 | supermarket-exterior | B | SUPERMARKET fascia is cut off by the canopy edge: the text is half hidden from the square | sheet-044 |
| 46 | supermarket-till | R | wall or ceiling lamp | sheet-044 |
| 47 | supermarket-shelves | B | gondola shelves in the hall are empty (#4901) | sheet-044 |
| 48 | supermarket-aisle | B | gondolas hold only baskets; goods stand on the dark wall units at the back (#4901) | sheet-048 |
| 49 | generalstore-exterior | M | HOUSEWARES fascia over the arch, hedge in front | sheet-048 |
| 50 | generalstore-till | M | till, scales, radio, posters, stocked shelf units, shopkeeper at the till | sheet-048 |
| 51 | generalstore-room | R | wall with a plant and a hand truck | sheet-048 |
| 52 | generalstore-safe | M | stocked wall shelves, ladder, crates, safe, shopkeeper | sheet-052 |
| 53 | butcher-exterior | M | BUTCHER fascia, kerb rail | sheet-052 |
| 54 | butcher-counter | B | long dark counter, bare floor, dark room (#4903) | sheet-052 |
| 55 | butcher-freezer | R | shopkeeper close to the lens | sheet-052 |
| 56 | butcher-chiller | B | chiller case and a crate, nothing on show (#4903) | sheet-056 |
| 57 | bakery-exterior | M | BAKERY fascia, kerb rail, cone | sheet-056 |
| 58 | bakery-counter | B | near-black counter, dark narrow room (#4903) | sheet-056 |
| 59 | bakery-rack | B | one rack, three loaves (#4903) | sheet-056 |
| 60 | hardware-exterior | M | HARDWARE board on a yard shed, counter and planks outside | sheet-060 |
| 61 | hardware-counter | R | wall or window | sheet-060 |
| 62 | hardware-shelf | R | wall or window | sheet-060 |
| 63 | hardware-toolbench | R | wall or window | sheet-060 |
| 64 | clinic-exterior | B | clinic board is small beside the clothing shop's (#4905) | sheet-064 |
| 65 | clinic-desk | M | clinic desk, typewriter, dividers, sofa; furnished | sheet-064 |
| 66 | clinic-beds | R | coat rack and a locker | sheet-064 |
| 67 | clinic-doctor | M | doctor in the room, clock, sofa | sheet-064 |
| 68 | police-exterior | M | POLICE fascia, UAZ at the kerb, road sign | sheet-068 |
| 69 | police-duty-desk | R | wall, door or switch | sheet-068 |
| 70 | police-booking | R | wall, door or switch | sheet-068 |
| 71 | police-wanted-board | R | wall, door or switch | sheet-068 |
| 72 | police-armory | M | dispatch board, lockers, desk with typewriter | sheet-072 |
| 73 | police-cell | M | cell with bench, bed, chequered floor | sheet-072 |
| 74 | dock-crane | M | quay crane, container, rails | sheet-072 |
| 75 | dock-containers | M | container yard and tally cabin | sheet-072 |
| 76 | dock-warehouse | R | pines in front of the building | sheet-076 |
| 77 | dock-smokehouse | M | smokehouse shed with its smoke, export board | sheet-076 |
| 78 | dock-wholesale | R | pines in front of the building | sheet-076 |
| 79 | harbour-office-sign | M | HARBOUR OFFICE plate on its cabin, phone booth | sheet-076 |
| 80 | harbor-water-edge | M | cranes and stacked containers at the quay edge | sheet-080 |
| 81 | brow-desk | B | desk and counter lost in knee-high grass; a rust-coloured plate stands upright (#4906) | sheet-080 |
| 82 | brow-furnace | R | wall and fence only | sheet-080 |
| 83 | brow-picks-ore | R | wall and fence only | sheet-080 |
| 84 | brow-supply-counter | B | supply counter and desk sunk in grass, no cleared ground (#4906) | sheet-084 |
| 85 | brow-hut | M | cabin on its deck with a miner at the door, ore cart | sheet-084 |
| 86 | brow-sign | M | COALMAN'S BROW MINE sign readable | sheet-084 |
| 87 | highstone-picks | R | back of the Highstone sign (blank by design; blank-sign fix is in flight) | sheet-084 |
| 88 | highstone-ore-rich | R | boulder | sheet-088 |
| 89 | highstone-sign | M | HIGHSTONE IRON MINE sign readable, fence | sheet-088 |
| 90 | highstone-cart-timber | M | cart, timber, rusty ore rocks, fence; sparse | sheet-088 |
| 91 | eastrise-picks | R | back of the East Rise sign | sheet-088 |
| 92 | eastrise-coal | B | the coal heap is grey gravel, not coal (#4906 notes) | sheet-092 |
| 93 | eastrise-sign | M | East Rise: sign, boulders, fence, cart | sheet-092 |
| 94 | eastrise-cart | M | East Rise: sign, boulders, fence, cart | sheet-092 |
| 95 | smelter-furnace | M | smelter yard: furnace, wheelbarrow, bucket, truck, silo (smelter fix is in flight) | sheet-092 |
| 96 | smelter-yard | R | fence, brush or trunk in the way | sheet-096 |
| 97 | smelter-sign | R | fence, brush or trunk in the way | sheet-096 |
| 98 | powerplant-coal-counter | B | coal buyer is a yellow box on dirt (#4907) | sheet-096 |
| 99 | powerplant-yard-truck | R | fence, brush or trunk in the way | sheet-096 |
| 100 | powerplant-heaps | M | gravel heaps, truck and brush at the yard edge | sheet-100 |
| 101 | powerplant-sign | R | fence, brush or trunk in the way | sheet-100 |
| 102 | sawmill-saw | M | sawmill hall: saw lines, boards, desk | sheet-100 |
| 103 | sawmill-desk | M | sawmill desk, rails, decks | sheet-100 |
| 104 | sawmill-boards-decks | M | sawmill desk, rails, decks | sheet-104 |
| 105 | sawmill-sign | M | SAWMILL sign on the hall front | sheet-104 |
| 106 | woodlot-saw | R | wall or room off the route | sheet-104 |
| 107 | woodlot-buyer-desk | M | woodlot shed: racks, planks, light through the roof | sheet-104 |
| 108 | woodlot-stacks | M | workbench, logs, loading dock | sheet-108 |
| 109 | woodlot-truck | R | wall or room off the route | sheet-108 |
| 110 | truckyard-row | M | truck row under the canopy | sheet-108 |
| 111 | truckyard-examiner | M | TRUCK SALES sign, examiner by his hut | sheet-108 |
| 112 | truckyard-gate | M | check gate beside the gravel bay | sheet-112 |
| 113 | truckyard-sign-hut | M | TRUCK SALES sign, examiner by his hut | sheet-112 |
| 114 | hostel-exterior | M | BUTCHER, HOSTEL and BAKERY fascias along one front | sheet-112 |
| 115 | hostel-fascia | M | HOSTEL fascia, door, curtains | sheet-112 |
| 116 | hostel-bed | R | door panel, window | sheet-116 |
| 117 | hostel-bunk | R | door panel, window | sheet-116 |
| 118 | hostel-ground-floor | M | hostel ground floor: tiled hall, stair, suitcase | sheet-116 |
| 119 | rt-square-n | M | square from its centre: church side, cobbles, a stool | sheet-119 |
| 120 | rt-square-e | M | square to the east: street, tree, kerb rail | sheet-119 |
| 121 | rt-square-s | M | square to the south: fence, trees, fuel canopy beyond | sheet-119 |
| 122 | rt-square-w | M | square to the west: obelisk, houses, no-entry sign | sheet-119 |
| 123 | rt-main-street-w | R | camera under the map; see 163 to 165 | sheet-123 |
| 124 | rt-main-street-e | R | camera under the map; see 163 to 165 | sheet-123 |
| 125 | rt-cafe-from-square | M | cafe side of the square from the cobbles | sheet-123 |
| 126 | rt-clerk-a | M | clerk behind the glass hatch, facing the hall | sheet-123 |
| 127 | rt-clerk-b | R | door leaf | sheet-127 |
| 128 | rt-hall-entrance | M | hall entrance: notice board, coat stand, glass doors | sheet-127 |
| 129 | rt-carlot-a | M | car lot from the slope: UAZ vans parked on the slab behind the retaining wall | sheet-127 |
| 130 | rt-carlot-b | R | water tower | sheet-127 |
| 131 | rt-carlot-c | M | car lot from the slope: UAZ vans parked on the slab behind the retaining wall | sheet-131 |
| 132 | rt-bank-ext-a | M | bank block with its red BANK board, a Ural and an estate car parked in the yard | sheet-131 |
| 133 | rt-bank-ext-b | M | bank block with its red BANK board, a Ural and an estate car parked in the yard | sheet-131 |
| 134 | rt-bank-hall | M | bank hall: counter, registers, teller, lockers | sheet-131 |
| 135 | rt-supermarket-till | M | till area: shopkeeper, counter, crates, glass doors | sheet-135 |
| 136 | rt-supermarket-shelf2 | B | aisle of empty gondola boards (#4901) | sheet-135 |
| 137 | rt-generalstore-shelf | R | wall or ceiling lamp | sheet-135 |
| 138 | rt-hardware-counter | M | hardware counter with clerk and "Hardware store" prompt | sheet-135 |
| 139 | rt-hardware-shelf | R | wall or window | sheet-139 |
| 140 | rt-clinic-beds | R | clothes shop shelf (gloves) next door | sheet-139 |
| 141 | rt-clinic-desk | M | clinic room: desk, rug, kitchen end | sheet-139 |
| 142 | rt-police-desk | M | police hall: officer, boards, desk with files | sheet-139 |
| 143 | rt-police-booking | M | booking desk and stair | sheet-143 |
| 144 | rt-police-wanted | R | wall, door or switch | sheet-143 |
| 145 | rt-dock-wholesale | M | wholesale wall, ladder, door, windows | sheet-143 |
| 146 | rt-dock-warehouse | R | pines in front of the building | sheet-143 |
| 147 | rt-brow-desk | R | boulder or trunk in the way | sheet-147 |
| 148 | rt-brow-furnace | B | green bench in the grass, props hidden (#4906) | sheet-147 |
| 149 | rt-brow-counter | B | counter and desk in grass, rusty plate upright (#4906) | sheet-147 |
| 150 | rt-highstone-picks | R | boulder or trunk in the way | sheet-147 |
| 151 | rt-eastrise-picks | M | East Rise: sign, boulders, fence, cart | sheet-151 |
| 152 | rt-smelter-furnace | M | smelter yard: furnace, wheelbarrow, bucket, truck, silo (smelter fix is in flight) | sheet-151 |
| 153 | rt-smelter-sign | R | truck roof | sheet-151 |
| 154 | rt-powerplant-counter | B | same yellow box on dirt (#4907) | sheet-151 |
| 155 | rt-powerplant-truck | R | fence, brush or trunk in the way | sheet-155 |
| 156 | rt-powerplant-sign | R | fence, brush or trunk in the way | sheet-155 |
| 157 | rt-hostel-bed | M | hostel room: bed, chair, table, plant, wardrobe | sheet-155 |
| 158 | rt-hostel-bunk | M | hostel room: bunk, divider shelf, curtain | sheet-155 |
| 159 | rt-garage-counter | B | garage shop: the mechanic faces a washroom corner (#4904) | sheet-159 |
| 160 | rt-orchard-juicer-side | M | juice press and desk from the barn side | sheet-159 |
| 161 | rt-butcher-wide | B | butcher counter bare (#4903) | sheet-159 |
| 162 | rt-bakery-wide | B | bakery: one loaf on the rack and counter (#4903) | sheet-159 |
| 163 | r3-main-street-w | M | main street: road, kerb, houses, garages, poles | sheet-163 |
| 164 | r3-main-street-e | M | main street: road, kerb, houses, garages, poles | sheet-163 |
| 165 | r3-main-street-mid | M | main street: road, kerb, houses, garages, poles | sheet-163 |
| 166 | r3-generalstore-shelf | M | stocked wall shelves, ladder, crates, safe, shopkeeper | sheet-163 |
| 167 | r3-generalstore-shelf2 | R | wall or ceiling lamp | sheet-167 |
| 168 | r3-garage-shop-wide | R | wall of the garage shop | sheet-167 |
| 169 | r3-brow-yard-wide | R | boulder and wall | sheet-167 |
