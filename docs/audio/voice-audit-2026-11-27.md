# Voice audit, 2026-11-27

Standing 0.3.0 rule: every NPC line a player can hear is voiced (ElevenLabs, one male voice per NPC) and its audio
says what the line says now. This is the audit behind `validate-npc-lines-voiced`, the check that keeps it true.

## Method

1. Every dialogue (`tools/assets/dialogue/*/dialogue.json`) against its recordings: `validate-dialogue-voices` already
   held, and still holds, **245 of 245** island lines voiced and current (hash of text and voice). All 405 caption
   strings equal their dialogue text, so the US English sweep of this week did not leave a stale recording behind.
2. The sources that feed a dialogue, which that check does not see: the keys a dialogue hook puts in `spoken`
   (a toast text the NPC says), a quest giver's description, hand-in and idle strings, rumor rows and announced keys.
   A key with no `for_key` line is shown as a caption with no voice (`'<key>' says ... as a caption only`).
3. The strings that changed since 2026-11-19 against the lines that speak them.

## Findings

- **#4931 market errand**: not a dialogue string; it is the quest description. But the clerk speaks it (`for_key`
  `#LF-Quest_EveronMarket_Desc`) and kept saying the old words, without "scroll to the juice row, the last one, and press F".
  Re-recorded (`clerk_everon/market`).
- **#4938 charge sheet**: the fine and "capped" row, the cell gate and the lot lock row are UI text and toasts, not NPC
  speech. No audio is owed; nothing changed.
- **50 speech sources had no recording** (table below): the clerk's settling-in chain, ID card and character-switch answers,
  the harbour master's contract refusals and shipment errands, the mayor's and old man's anniversary errands, the tally
  clerk's fee answers, the informant's five answers, "nothing new" and "asked today" for every rumor giver, the flag wall
  registrar's refusals, and "Take a look. Cash only." for the eight role shopkeepers. Each now speaks in its NPC's own voice.
- Lines that paraphrase a string (`for_key` lines) are now pinned to it (`key_pin`, or `pin_key` to name another string:
  the foremen follow the unlock rows, not the `Next_NN` fallback). A string that changes fails `validate` until the
  line is re-read and `validate-npc-lines-voiced --update` is run; this is what would have caught #4931.

## Lines (NPC, line, string key, before, after)

| NPC | Line | Key | Before | After |
|---|---|---|---|---|
| anniversary_mayor | `chairs` | `#LF-Quest_AnniversaryChairs_Desc` | no line, no audio (the key was shown as a caption only) | voiced: 97 chars |
| anniversary_mayor | `flowers` | `#LF-Quest_AnniversaryFlowers_Desc` | no line, no audio (the key was shown as a caption only) | voiced: 87 chars |
| barkeeper | `rumor_nothing_new` | `#LF-RumorTalk_NothingNew` | no line, no audio (the key was shown as a caption only) | voiced: 29 chars |
| barkeeper | `rumor_asked_today` | `#LF-RumorTalk_AskedToday` | no line, no audio (the key was shown as a caption only) | voiced: 54 chars |
| clerk_everon | `papers_first` | `#LF-Quest_ArrivalRegister_Desc` | no line, no audio (the key was shown as a caption only) | voiced: 86 chars |
| clerk_everon | `market_done` | `#LF-Quest_EveronMarket_TurnIn` | no line, no audio (the key was shown as a caption only) | voiced: 164 chars |
| clerk_everon | `settle_parcels` | `#LF-Quest_SettleParcels_Desc` | no line, no audio (the key was shown as a caption only) | voiced: 111 chars |
| clerk_everon | `settle_fuel` | `#LF-Quest_SettleFuel_Desc` | no line, no audio (the key was shown as a caption only) | voiced: 90 chars |
| clerk_everon | `settle_wheels` | `#LF-Quest_SettleWheels_DescLicence` | no line, no audio (the key was shown as a caption only) | voiced: 120 chars |
| clerk_everon | `settle_permit` | `#LF-Quest_SettlePermit_Desc` | no line, no audio (the key was shown as a caption only) | voiced: 85 chars |
| clerk_everon | `settle_fish` | `#LF-Quest_SettleFish_Desc` | no line, no audio (the key was shown as a caption only) | voiced: 45 chars |
| clerk_everon | `settle_plot` | `#LF-Quest_SettlePlot_Desc` | no line, no audio (the key was shown as a caption only) | voiced: 86 chars |
| clerk_everon | `settle_plot_done` | `#LF-Quest_SettlePlot_TurnIn` | no line, no audio (the key was shown as a caption only) | voiced: 64 chars |
| clerk_everon | `settle_foundation` | `#LF-Quest_SettleFoundation_Desc` | no line, no audio (the key was shown as a caption only) | voiced: 78 chars |
| clerk_everon | `card_replaced` | `#LF-IdCard_ClerkReplaced` | no line, no audio (the key was shown as a caption only) | voiced: 70 chars |
| clerk_everon | `card_still_carried` | `#LF-IdCard_ClerkStillCarried` | no line, no audio (the key was shown as a caption only) | voiced: 57 chars |
| clerk_everon | `card_no_cash` | `#LF-IdCard_ClerkNoCash` | no line, no audio (the key was shown as a caption only) | voiced: 62 chars |
| clerk_everon | `card_never_issued` | `#LF-IdCard_ClerkNeverIssued` | no line, no audio (the key was shown as a caption only) | voiced: 61 chars |
| clerk_everon | `switch_no` | `#LF-CharSwitch_ClerkNo` | no line, no audio (the key was shown as a caption only) | voiced: 38 chars |
| clerk_everon | `switch_yes` | `#LF-CharSwitch_ClerkYes` | no line, no audio (the key was shown as a caption only) | voiced: 44 chars |
| clerk_everon | `market` | `#LF-Quest_EveronMarket_Desc` | audio said the old words (string changed in #4931) | re-recorded |
| harbour_master | `shipment_count_desc` | `#LF-Quest_ShipmentCount_Desc` | no line, no audio (the key was shown as a caption only) | voiced: 77 chars |
| harbour_master | `contract_unknown` | `#LF-Contract_Unknown` | no line, no audio (the key was shown as a caption only) | voiced: 30 chars |
| harbour_master | `contract_not_able` | `#LF-Contract_NotAble` | no line, no audio (the key was shown as a caption only) | voiced: 36 chars |
| harbour_master | `shipment_sold_desc` | `#LF-Quest_ShipmentSold_Desc` | no line, no audio (the key was shown as a caption only) | voiced: 86 chars |
| night_shift | `informant_never_heard` | `#LF-Informant_NeverHeard` | no line, no audio (the key was shown as a caption only) | voiced: 28 chars |
| night_shift | `informant_no_cash` | `#LF-Informant_NoCash` | no line, no audio (the key was shown as a caption only) | voiced: 53 chars |
| night_shift | `informant_told` | `#LF-Informant_Told` | no line, no audio (the key was shown as a caption only) | voiced: 86 chars |
| night_shift | `informant_already` | `#LF-Informant_Already` | no line, no audio (the key was shown as a caption only) | voiced: 44 chars |
| night_shift | `informant_no_spot` | `#LF-Informant_NoSpot` | no line, no audio (the key was shown as a caption only) | voiced: 46 chars |
| night_shift | `rumor_nothing_new` | `#LF-RumorTalk_NothingNew` | no line, no audio (the key was shown as a caption only) | voiced: 29 chars |
| night_shift | `rumor_asked_today` | `#LF-RumorTalk_AskedToday` | no line, no audio (the key was shown as a caption only) | voiced: 54 chars |
| old_man | `letter_desc` | `#LF-Quest_AnniversaryLetter_Desc` | no line, no audio (the key was shown as a caption only) | voiced: 85 chars |
| old_man | `letter_turn_in` | `#LF-Quest_AnniversaryLetter_TurnIn` | no line, no audio (the key was shown as a caption only) | voiced: 47 chars |
| registrar | `flag_same` | `#LF-OrgFlagWall_Same` | no line, no audio (the key was shown as a caption only) | voiced: 34 chars |
| registrar | `flag_no_gang` | `#LF-OrgFlagWall_NoGang` | no line, no audio (the key was shown as a caption only) | voiced: 30 chars |
| registrar | `flag_not_leader` | `#LF-OrgFlagWall_NotLeader` | no line, no audio (the key was shown as a caption only) | voiced: 42 chars |
| registrar | `flag_refused` | `#LF-OrgFlagWall_Refused` | no line, no audio (the key was shown as a caption only) | voiced: 27 chars |
| role_fence_everon | `show_goods_reply` | `#LF-Shopkeeper_ShowGoodsReply` | no line, no audio (the key was shown as a caption only) | voiced: 23 chars |
| role_mechanic_everon | `show_goods_reply` | `#LF-Shopkeeper_ShowGoodsReply` | no line, no audio (the key was shown as a caption only) | voiced: 23 chars |
| role_medic_everon | `show_goods_reply` | `#LF-Shopkeeper_ShowGoodsReply` | no line, no audio (the key was shown as a caption only) | voiced: 23 chars |
| role_police_everon | `show_goods_reply` | `#LF-Shopkeeper_ShowGoodsReply` | no line, no audio (the key was shown as a caption only) | voiced: 23 chars |
| role_store_clerk_everon | `show_goods_reply` | `#LF-Shopkeeper_ShowGoodsReply` | no line, no audio (the key was shown as a caption only) | voiced: 23 chars |
| role_teller_everon | `show_goods_reply` | `#LF-Shopkeeper_ShowGoodsReply` | no line, no audio (the key was shown as a caption only) | voiced: 23 chars |
| role_townsfolk_everon | `show_goods_reply` | `#LF-Shopkeeper_ShowGoodsReply` | no line, no audio (the key was shown as a caption only) | voiced: 23 chars |
| role_trader_everon | `show_goods_reply` | `#LF-Shopkeeper_ShowGoodsReply` | no line, no audio (the key was shown as a caption only) | voiced: 23 chars |
| shopkeeper_everon | `rumor_nothing_new` | `#LF-RumorTalk_NothingNew` | no line, no audio (the key was shown as a caption only) | voiced: 29 chars |
| shopkeeper_everon | `rumor_asked_today` | `#LF-RumorTalk_AskedToday` | no line, no audio (the key was shown as a caption only) | voiced: 54 chars |
| tally_clerk | `fee_short` | `#LF-Dialogue_FeeShort` | no line, no audio (the key was shown as a caption only) | voiced: 51 chars |
| tally_clerk | `fee_held` | `#LF-Dialogue_FeeHeld` | no line, no audio (the key was shown as a caption only) | voiced: 38 chars |
| tally_clerk | `fee_failed` | `#LF-Dialogue_FeeFailed` | no line, no audio (the key was shown as a caption only) | voiced: 58 chars |

All other lines (245): voiced and current before and after; unchanged.

## Still without a voice, and why

| Source | Why |
|---|---|
| `LF_DialogueHook_StandingFile` (the clerk's "what is in my file?") | The words are composed at run time from the player's standing and tier words; there is no fixed line to record. Needs a set of fixed phrases (a design change, not a freeze fix). |
| Role conversations `farmer`, `logger`, `miner`, `office`, `ranger` (`LF-NpcRole_*`) | The runtime-built caption tree. No pack prefab uses these roles, so no player hears them; `validate-dialogue-voices` fails the day one is placed. |
| Toasts worded as an NPC's speech with no dialogue behind them: `LF-CounterOutlet_Police` (barkeeper), `LF-BuyerSale_Closed/Nothing`, `LF-Witness_Unsure1-4`, `LF-Garage_SlotsFull`, `LF-Launder_*` | They are notifications raised by a feature, not conversation lines, and the NPC may be absent or the line may come from a prop. Voicing them needs a new call (`LF_Dialogue.Announce`) beside each toast, which is a feature change; listed for the post-freeze list. |
| `LF-Store_ClerkThief` for store staff whose dialogue is not `shopkeeper` | One recorded line exists (the shopkeeper); each staff NPC with another dialogue stays silent (the hook logs `not said`). |
| `show_goods_reply` on the eight role shopkeepers | Recorded and wired, but the island's placed role NPCs offer no shop reply, so the in-game proof could not reach it; the hook is covered by the check. |

## The check

`tools\cli validate` runs `validate-npc-lines-voiced` (`tools/lib/npc-lines-voiced.mjs`): hooks declare what they speak with
`// lf-spoken: #LF-Key` (or `// lf-not-spoken:` for system captions); each dialogue that uses the hook, each quest giver's
strings, each rumor row and each `LF_Dialogue.Announce` needs a recorded `for_key` line; every `for_key` line has a
current pin. The selftest plants each gap. `node tools/validation/validate-npc-lines-voiced.mjs --report` prints coverage.

## Proof

Full-profile island session with sound (`node agent-box/session.mjs npc-voice-proof`): the client console shows the new
lines starting with valid sound handles.

```
[LFDebug:Dialogue] line 'barkeeper/rumor_asked_today' plays: event SOUND_DLG_BARKEEPER_RUMOR_ASKED_TODAY, 3.56 s, mouth envelope 72 steps
[LFDebug:Dialogue] line 'shopkeeper/rumor_nothing_new' plays: event SOUND_DLG_SHOPKEEPER_EVERON_RUMOR_NOTHING_NEW, 1.67 s, mouth envelope 34 steps
[LFDebug:Dialogue] line 'tally_clerk/fee_short' plays: event SOUND_DLG_TALLY_CLERK_FEE_SHORT, 2.97 s, mouth envelope 60 steps
```

An agent cannot hear: the maintainer should listen to the wavs under `addons/LF_Everon1989/Sounds/Dialogue/` listed by `git status` (new `*_<line>.wav`).
