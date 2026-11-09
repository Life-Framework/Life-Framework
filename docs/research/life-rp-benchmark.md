# Life/RP benchmark

What the life and roleplay genre built over fifteen years, what kept its
players, what drove them away, and what that means for Life Framework after M1.
Researched 2026-11-08. Read it with [direction](../direction.md),
[decisions](../decisions.md), and [M1](../roadmap/m1-playtest-town.md).

**Nothing here was imported.** This is a design benchmark. No code, config, or
text from any project below is in this repo, and none should be.

| Project | Licence | What we may do |
| --- | --- | --- |
| Altis Life (AsYetUntitled/Framework, Tonic's original) | CC BY-NC-ND 4.0 (README and `LICENSE.md`); the original was CC BY-NC-ND 3.0 [1][2] | Read for ideas. No derivatives, no commercial use |
| Takistan Life (Armitxes) | Custom: use, modify, and redistribute with credit [3] | Ideas only (Arma 2 SQF, nothing to port anyway) |
| ESX, QBCore, Qbox, ox_inventory, NPWD, qb-phone | GPL-3.0 [G] | Ideas only. GPL code must never enter an MIT repo |
| ox_core, ox_lib | LGPL-3.0 in `LICENSE`, but ox_core's `NOTICE.md` says GPL-3.0-or-later [G] | Ideas only |
| Team ELAN – Arma Life (Reforger) | APL-ND [E1] | Functionality reference only. No derivatives |
| Everon Life (Reforger) | MIT [E3] | Our upstream (`EL_` classes); see `docs/everonlife-forks.md` |

**Confidence.** Facts with a source number were read on that page. Lines marked
*(recollection)* are widely repeated genre knowledge that no primary page
confirmed during this research; treat them as hypotheses. Reddit was not
reachable from the research tools, and the Team ELAN wiki was read only through
search-engine excerpts.

## 1. Feature matrix

Life Framework column: **built** = in the tree today (see
[features](../features.md)); **M1** = an M1 scope; **planned** = a recorded
decision or an after-release scope; **absent** = no plan.

| Feature | Altis Life (A3) | GTA RP (ESX / QB / NoPixel) | Elan Life (Reforger) | Life Framework |
| --- | --- | --- | --- | --- |
| **Jobs** | Gather → process → sell chains (apples, copper, salt, oil, diamonds); delivery missions, fishing, hunting [1][8] | Job grades with pay per grade; bus, trucker, tow, garbage, vineyard, reporter, judge, lawyer [G]; hidden job "rep" on big servers *(recollection)* | Professions (miner, blacksmith, tailor, crafter, drug dealer) gated by 11 visible skills to level 20; $50k to switch, 24 h cooldown [E4] | **built**: courier, fuel delivery, taxi, gather/process, mining. **M1**: pay at the drop. **planned**: hidden experience (decision) |
| **Economy** | Bank is safe, cash is stealable; ATMs; fixed or server-tuned prices [1][8] | Cash plus bank accounts, job/gang "society" accounts, bank cards [G] | Global dynamic market: selling a good drops its price, random market events [E5]; auction house [E9] | **built**: cash items, ATM, shops, traders, unemployment benefit. **planned**: supply/demand prices (decision) |
| **Housing** | Buy house, garage, placeable containers; raidable, police can break doors [1] | Houses with keys, stash, garage, outfits; apartments [G] | Housing listed; town lockers (200 items, 2 weapons) [E9] | **built**: org hideout (door, machine). **M1**: hideout stash. **planned**: buy and furnish (decision) |
| **Vehicles** | Garages, keys, impound, chop shop, colour shop [1] | Vehicle shop, keys, garages, impound, customs, mechanic; car boosting contracts with VIN scratching *(recollection; sold as paid scripts [7c])* | Starter vehicle from a garage [E10]; fuel trucks for goods [E5] | **built**: lot, keys, plates, lock, garage store/retrieve, vehicle robbery. **M1**: impound for ungaraged cars |
| **Crime types** | Rob players, illegal processing (weed, cocaine, heroin), rebel licence; servers add meth, organ theft, uranium [1][11] | Store and house robbery, drugs, trap houses, street racing [G] | Drugs grown/processed, gas station robbery [E9] | **built**: till, player, weapon, and vehicle robbery; breach; contraband run; hospital cabinet |
| **Heists** | Federal Reserve: blasting charge, bolt cutters, ≥5 cops, 20 min bomb, police in waves, 35-50 min cooldown [14]; Blackwater, jail, evidence lockup [9] | Fleeca, Paleto, Pacific, jewellery, power station; tool items (thermite, drills, USB, electronic kit) and hacking minigames [G] | Evidence locker raid, prison break [E9] | **built**: bank vault with carried bags. **M1**: louder alarm with no police. **planned**: jailbreak scope |
| **Police loop** | Restrain, escort, search, seize, tickets, wanted list, bounties, spike strips, radar, jail and bail [1] | Job grades, MDT, evidence (casings, blood, fingerprints, DNA), ANPR, cameras, prison [G] | Citations, arrest, jail, evidence locker, divisions, academy, air unit; fixed fine schedule [E2][E6] | **built**: duty, arrest, escort, jail, ranks, wanted, confiscate, spike strip. **M1**: duty desk, wanted board, paper ticket, jail slip, dispatch, dual inventory |
| **EMS / medical** | Medic faction, revive (servers: R&R) [1][9] | qbx_medical, ambulance job, hospital [G] | None found | **M1**: wake in hospital, bill from the bank. **planned**: revive. **absent**: medic job |
| **Courts / lawyers** | Olympus courthouse [9] | Judge and lawyer jobs, DOJ on big servers [G] *(DOJ depth: recollection)* | Fines and sentences fixed by "the government"; officers cannot change them [E6] | **absent** |
| **Gangs / orgs** | Create, invite, upgrade; servers add cartels (10% tax on a drug's sales, passive income), turfs, gang bases, gang wars, weekly Conquest [10][15] | Gang accounts, multi-gang membership [G]; turf and graffiti *(recollection)* | Gangs with bank accounts; capturable "Group Spots" [E9] | **built**: organizations, fund, processing upgrade, hideout. **M1**: registry desk, invite by facing |
| **Phone / comms** | Cellphone texts, news broadcast [1] | Full phone UI: banking, garages, crypto, races, police app, camera [G] | None found | **M1**: vanilla radio dispatch. **planned**: payphones and pagers (decision). No phone UI by design |
| **Identity** | Names over heads; licences in a menu [1] | Multicharacter, ID card item, city hall licences [G] | Licences barely visible; reputation score [E7] | **M1**: registration form, ID card, introductions with close-range names |
| **Progression** | Licences as the gate; servers add prestige and stats *(recollection)* | Job grades; hidden rep *(recollection)* | 11 visible skills, levels, reputation that changes shop prices [E3][E7] | **built**: levels, police ranks, licence catalog. **M1**: persists. **planned**: hidden experience, titles |
| **Events** | Airdrops, conquest, HQ takeover, invasion [9][15] | Seasonal scripted content (Prodigy) [G] | An events-only server [E11] | **planned**: world events after release |
| **Admin tools** | Admin menus; staff-run rules [16] | txAdmin-era tooling *(recollection)* | Ticket-based support; players call it slow [E12] | **built**: admin chat `!wadd`, dev gate. **absent**: moderation toolkit |
| **Anti-cheat / dupe** | SpyGlass, BattlEye filters [1] | Server authority varies by resource *(recollection)* | Not documented | **built**: server authority, dupe fixes (#79), range checks (#73); BattlEye from the engine |
| **Whitelist** | Cop slots for vetted veterans [5][8] | Whole-server whitelist with applications, sometimes paid to apply [7] | Police: Discord-verified, 50+ hours, 75-word in-character application [E2]; one whitelisted server [E11] | **M1**: police whitelist checked at the duty desk |

## 2. What made each succeed, and what hurt

### Altis Life (Arma 3, 2013–today)

**What worked**

- **Loss creates stakes.** John VanderZwet (Asylum): "If there's one thing that
  can make PvP exciting, it's consequence… the fear of loss will get your
  adrenaline pumping." Players start with roleplay and drift into crime [5].
- **Boundaries enable freedom.** Tonic: "People look for freedom but they still
  need the boundaries" [5].
- **A tentpole crime that everyone knows.** The Federal Reserve was "getting
  robbed more or less all the time" in 2014 [7]. Olympus still runs it as one of
  four "federal events" with item gates and police waves [14].
- **Gang economics last.** Cartels pay passive income and tax drug sales. In
  2026 Olympus is still tuning "Cartel Prime Time", run-proficiency bonuses that
  cut grind, and police wave rules [10][13]. Olympus has posted 313 monthly
  updates [12]. A decade of tuning a few loops beats adding new ones.

**What hurt**

- **Cop culture turned adversarial.** A 2014 cop diary describes "how easily our
  police could descend into an antagonistic us-against-them culture" [21].
- **Civilian life thinned out.** One estimate: about 70% of players picked cop or
  criminal [22]. Anecdotal, but every later server added civilian content to
  fix it.
- **Admin abuse and donor perks.** Players left servers over arbitrary bans
  [22]. Bohemia's monetisation rules (29 Jan 2015) ban perks that give money,
  weapons, vehicles, discounts, or housing [23][25]. Olympus+ still sells
  monthly gear and vehicle airdrops [18].
- **Exploits and grind** *(recollection)*: dupes, injected scripts, and hauling
  raw materials for hours.

### GTA RP (FiveM, 2017–today)

**What worked**

- **Characters, not loops, make the stories.** Kotaku credits streamers who
  "creat[ed] their own dramas" and characters crossing paths by accident, such
  as a burger-shop owner who refused to commit crime [K]. NoPixel 3.0 (Feb 2021)
  pushed GTA V to a 438K Twitch viewer peak [S].
- **Depth in every role.** 3.0 added driving instructors, mechanics, and judges
  on top of crime and police [K]. Police have evidence (casings, blood,
  fingerprints, DNA), cameras, and ANPR [G]. A cop can investigate, not only
  chase.
- **Tiered heists with physical tools.** Fleeca to Pacific needs thermite,
  drills, and electronics kits [G]. Every tier is a supply chain for the tier
  below.

**What hurt**

- **Admin favouritism and ban drama.** xQc was banned four or five times in 2021.
  Shroud and summit1g questioned bias [X]. In 2023 Penta quit, accusing admins
  of enabling abuse, and management changed hands [P].
- **Pay-to-apply.** Paid "review boosts" moved donors to the front of the queue
  [7].
- **Grind after a reset.** NoPixel 4.0 reset the economy and drew "too much
  grind, not enough fun" reviews (low-confidence source) [N].
- **Platform risk.** Rockstar banned the FiveM developers in 2015, bought Cfx.re in
  2023, and now restricts monetisation [R].

### Elan Life (Reforger, 2022–today)

Elan Life is our closest neighbour: Team ELAN, ex-Altis Life operators, about 7
servers of up to 128 players, version 0.10.5 updated 7 Nov 2026, 26k downloads,
78% rating [E1][E11]. The maintainer judges its taste weak; this section only
records what exists and what players say.

- **Its functionality echoes Altis Life:** processing chains, a global dynamic
  market, visible skills, gangs with capture spots, gas station robbery, evidence
  locker raids, and prison breaks [E3][E5][E9].
- **What players report:** players report console RDM, slow admin tickets,
  "nobody roleplays", and "basically Wasteland". They advise joining the
  whitelisted server [E12]. Solo play "leads nowhere" [E12].
- **What's missing:** no medic, phone, courts, or visible licences [E1–E12].
- **The lesson:** the complaints are about moderation, griefing, and having
  nothing to do alone, not missing features.

### Patterns across all three

1. **Moderation decides whether a server lives.** RDM and admin conduct appear in
   every burnout story; feature gaps rarely do.
2. **Few loops, tuned for years**, outlast many shallow ones (Olympus, 313
   updates).
3. **Every role needs something to do when the other side is absent.** Bored
   cops turn on players; lone civilians log off.
4. **Pay-to-win or pay-to-access poisons trust**, and platform owners eventually
   forbid it.
5. **Loss makes stakes, but grind after loss makes quitters.**

## 3. Gaps and opportunities

### Adopt, and how it exists in the world

| Gap | Genre version | Our in-world version |
| --- | --- | --- |
| EMS | Medic faction with a revive menu | A downed state before death. Reforger already simulates bleeding, tourniquets, morphine, and saline. A medic job carries a real kit, stabilises the patient, and drives the ambulance to the hospital bed from M1-26. Not stabilised in time: you wake in hospital with the bill |
| Investigation | MDT, DNA, fingerprints | No computers in 1989. A payphone call gives a vague description ("man in a green jacket, tan Škoda, plate starts B-") on the station board. Casings and a dropped item are physical evidence bagged into the evidence locker. Plates come from `LF_LicensePlateManager` |
| Comms | Phone UI with apps | Payphones (call a service, anonymous tip), pagers for jobs and police, Reforger radios on frequencies. Interception (a scanner) is a crime tool |
| Heist ladder | Fleeca → Pacific with tool items | Till → corner shop → cash van → bank → the armory. Each tier needs a real tool or intel item from the tier below or from contraband: drill, blasting charge, keycard, guard rota on paper. M1's alarm rules stay |
| Licences | City hall menu | Card items issued at a town-hall desk. The gun shop sells serialised weapons to licence holders. Serials feed investigation |
| Civilian depth | Businesses, mechanics, judges | Player-run businesses: a bar, a garage, a store stocked from player-made goods, with a till that can be robbed |
| Gang territory | Cartels with capture bars | A protection racket on a business, a hideout others can find, contraband routes that pay more when held. No capture-bar UI; control is who stands there and who gets paid |
| Moderation | Ticket queue, admin menu | An incident log the server keeps (deaths, damage, cash moves with who and where), readable by admins in Game Master. Rules are cheaper to enforce when facts are recorded |

### Skip on purpose

- **Phone UI and apps.** Decided: period comms only.
- **Floating name tags and a Y-menu hub.** Decided: introductions, one
  interaction menu.
- **Visible XP and skill trees** (Elan's 11 skills). Decided: hidden experience.
- **Hacking minigames as screen puzzles.** A drill that takes time, makes noise,
  and can jam is in-world and gives police time to arrive.
- **Paid whitelists, priority queues, donor gear.** Violates Bohemia's policy and
  the genre's trust lesson [25][R].
- **Global "war zone" exemptions from the rules** (Olympus cartels): see
  question 1.
- **A fixed government fine schedule** (Elan). Our ticket is written face to face;
  an officer judges the amount within a config range.

### Where Reforger lets us do what no life mode has done

- **Radio as gameplay.** Directional, range-limited radio is native to Reforger.
  Police dispatch can be overheard, jammed, or answered by the wrong person. No
  phone app can produce that tension.
- **Medical simulation.** Reforger's wound and bleeding model makes EMS a skill,
  not a button.
- **Vehicles and physics.** Real damage, fuel, towing, and spike strips (built)
  make the pursuit and the mechanic job matter.
- **1989 and no GPS.** Paper maps, plates read by eye, descriptions from
  witnesses. Investigation becomes human, which is what makes stories worth
  filming.
- **Game Master.** Reforger ships a live admin and event tool. Events and
  moderation can build on it instead of a custom admin panel.
- **Workshop and consoles.** Mods download automatically and reach Xbox and PS5
  players. That is reach Altis Life never had. It also means input without a
  keyboard (see question 8).
- **Proof tooling.** No life mode tests itself on a dedicated server with a
  visual harness. Fewer dupes and fewer broken loops is our moderation
  multiplier.

### Top 10 roadmap candidates for M2+

Ranked by how much each fixes a cause of community death or creates stories,
weighed against what it builds on.

1. **Incident log and Game Master moderation tools.** Moderation killed more
   servers than missing features; facts make rulings fast and fair.
2. **Downed state, medic job, and ambulance.** A third role with its own loop.
   Reforger's medical sim does half the work, and M1-26 gives it a destination.
3. **Payphones, pagers, and the station dispatch board.** The decided period
   comms. Every service (police, medic, taxi, mechanic) needs a way to be called.
4. **Witness descriptions and physical evidence.** Gives police something to do
   between chases and makes the vague-description wanted system real.
5. **Heist ladder with tool items.** A tentpole crime per tier (the Fed Reserve
   lesson), each feeding the next, on top of the M1 bank and alarm.
6. **Licence cards, gun shop, serialised weapons.** The decided licensed-weapons
   rule; serials connect crime to investigation.
7. **Buy and furnish property.** Decided. The longest-lived retention loop in
   both Altis and GTA RP.
8. **Supply/demand prices and trade routes.** Decided. Stops grind exploits and
   gives the contraband run and trucking jobs a reason to move.
9. **Player-owned businesses.** The fix for "70% pick cop or criminal" and for
   solo play leading nowhere. Uses the shop, till, and organization code.
10. **Organization rackets and routes.** Gang content that stays in the world:
    protect a business, hold a route, defend the hideout. Altis cartels' payoff
    without a capture bar.

Just below the cut: AI police when nobody is on duty (decided after M1), world
events on Game Master, a mechanic and tow job with impound, and courts.

## 4. Open taste questions for the maintainer

1. **Is any of Everon lawless?** Altis servers kept war zones and cartels where
   rules about initiating combat did not apply. That concentrated PvP away from
   town and gave gangs a home. Do we keep one law everywhere, or mark a place
   (a quarry, the islands) where the rules say "shoot first is allowed"?
2. **Do we encode "hostile intent" in a system or leave it to admin rules?**
   Every lasting community wrote initiation rules (talk first, 5 seconds to
   react). A sandbox could make it a mechanic instead: a hold-up action, or a
   raised weapon that marks intent, which the incident log records. Is that
   friction, or the rule we need?
3. **Should big crime have a gate?** NoPixel gated serious crime behind
   applications; Olympus gated it behind items and police count; we chose
   "always allowed, louder alarm". Does the top tier (armory, cash van) need
   reputation or an introduction from a fence, or is it open to anyone with the
   tools?
4. **What does death cost beyond the bill?** GTA RP has character kills and
   "value of life"; Altis has the New Life Rule (no return for 15 minutes or
   1 km). Do we want a rule that you forget the fight that killed you, a
   permanent character death by consent, or only the bill?
5. **Scheduled PvP or only emergent conflict?** Olympus runs Conquest four times
   a week with a cash pot, and it is one of its longest-lived features. Is a
   scheduled event like a cross-country rally, a gang war night, or a strike at
   the factory part of the flagship, or does that break the sandbox?
6. **How much of a court do we build?** Elan fixes fines by law; GTA RP has
   judges and lawyers. Ours is an officer writing a ticket. Do we add an appeal
   or a bail desk where another player decides, or leave justice to the officer
   and the config?
7. **Will the flagship take cosmetic donations?** The framework is never
   monetised. Bohemia allows cosmetics and reserved slots on registered
   Reforger servers [B]. When M4 runs, is the flagship strictly free, or
   cosmetic-only within Bohemia's policy?
8. **Are console players first-class on the flagship?** Reforger reaches Xbox
   and PS5, and Elan players blame console players for RDM [E12]. Typing a name,
   an amount, or a ticket is harder with a controller. Do we design every
   interaction for a gamepad first, or accept a PC-first flagship?

## Sources

[G] GitHub repositories read via `gh api` on 2026-11-08: `esx-framework/esx_core`,
`qbcore-fivem/qb-core` (and qb-phone, qb-banking, qb-management, qb-inventory,
qb-houses, qb-apartments, qb-policejob, qb-bankrobbery), `Qbox-project/qbx_core`,
`overextended/ox_core`, `ox_inventory`, `ox_lib`, `ox_mdt`, `ox_banking`,
`project-error/npwd`.

1. https://github.com/AsYetUntitled/Framework (README, LICENSE.md, file tree)
2. https://github.com/88tom/Altis-Life
3. https://github.com/Armitxes/TakistanLife
5. https://www.pcgamer.com/how-arma-3s-altis-life-mod-brought-pvp-and-roleplaying-together (2018-09-25)
7. https://www.pcgamer.com/mod-of-the-week-altis-life-for-arma-3 (2014-03-30); paid-review queue: https://progameguides.com/gta/how-to-join-the-nopixel-server-in-gta-v-rp/; [7c] https://forum.cfx.re/t/qbcore-esx-carboosting-vin-scratching/4819300
8. https://www.gamestar.de/artikel/altis-life-fuer-arma-3-aepfel-drogen-und-rebellen,3055791.html (2014-05-18)
9. https://wiki.olympus-entertainment.com/wiki/Main_Page
10. https://wiki.olympus-entertainment.com/wiki/Cartels
11. https://wiki2.gaming-asylum.com
12. https://olympus-entertainment.com/blogs/entry/313-altis-life-update-2026-09-11-september-content-update
13. https://olympus-entertainment.com/blogs/entry/312-roundtable-summary-09052026
14. https://wiki.olympus-entertainment.com/wiki/Federal_Reserve
15. https://wiki.olympus-entertainment.com/wiki/Conquest
16. https://wiki.olympus-entertainment.com/wiki/Altis_Life_Rules
18. https://wiki.olympus-entertainment.com/wiki/Olympus_Plus
21. https://gamedeveloper.com/game-platforms/respect-my-authority-role-playing-as-a-cop-in-arma-3 (2014-09-30)
22. https://steamcommunity.com/app/33930/discussions/0/135512305401937455
23. https://forums.bohemia.net/forums/topic/177973-arma-3-server-monetization/
25. https://www.bistudio.com/monetization (now redirects to bohemia.net; quoted via secondary sources)

- [B] https://reforger.armaplatform.com/news/server-monetization ; https://hosthavoc.com/blog/arma-reforger-server-monetization (2025-06-09)
- [K] https://kotaku.com/grand-theft-auto-role-playing-is-big-on-twitch-again-s-1846240147
- [S] https://streamscharts.com/news/gta-v-rp-server-takes-game-top-twitch-february
- [X] https://win.gg/news/shroud-considers-bias-on-nopixel-server-after-xqc-gets-third-ban/ ; https://sportskeeda.com/gta/someone-lose-everything-summit1g-questions-nopixel-gta-rp-server-s-ban-policy
- [P] https://www.gtaboom.com/penta-burns-bridges-on-his-way-out-of-nopixel-6ac5 ; https://www.gtaboom.com/nopixel-management-goes-from-koil-to-buddha-amidst-controversies-6126
- [N] https://fivemx.com/gta-news/nopixel-failed (reseller site, low confidence)
- [R] https://gamedeveloper.com/business/rockstar-allows-roleplaying-in-i-gta-online-i-but-no-loot-boxes-or-nfts ; https://www.club386.com/rockstar-acquires-gta-v-and-rdr-roleplaying-mod-maker-cfx-re/ ; https://www.gameserverkings.com/knowledge-base/fivem/server-monetisation/
- [E1] https://reforger.armaplatform.com/workshop/599820634D266D00-TeamELAN-ArmaLife-Main (and `/changelog`)
- [E2] https://wiki.team-elan.com/Police ; https://wiki.team-elan.com/Police_Application (search excerpts)
- [E3] https://wiki.team-elan.com/index.php?title=Skills (search excerpts); Everon Life: https://reforger.armaplatform.com/workshop/59636E668EA37AD7-EveronLife
- [E4] https://wiki.team-elan.com/Professions (search excerpts)
- [E5] https://wiki.team-elan.com/Dynamic_market (search excerpts)
- [E6] https://wiki.team-elan.com/Laws (search excerpts)
- [E7] https://wiki.team-elan.com/index.php?title=Reputation (search excerpts)
- [E9] https://wiki.team-elan.com/Group_Spots ; https://wiki.team-elan.com/Gas_station_robbery ; https://wiki.team-elan.com/Category:Features (search excerpts)
- [E10] https://steamcommunity.com/sharedfiles/filedetails/?id=3194800812 (search excerpt)
- [E11] https://wiki.team-elan.com/How-to-join (search excerpts)
- [E12] https://steamcommunity.com/app/1874880/discussions/12/604144884892359517/
