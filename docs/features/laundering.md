# Laundering

Dirty money (M7, parent #1025; framework #644). Money stays physical: there is no dirty balance.
Stolen money is a separate item that never counts as cash.

### `LF_MarkedCash` — `Feature/Laundering/LF_MarkedCash.c` (#2850)
- ✅ Marked notes are a stack of the `markedcash` prefab slot (`LF_PrefabSlots.conf`; core prefab
  `Prefabs/Items/Money/LF_MarkedCash.et`, inherits the cash `MoneyStack`, name `#LF-Item_MarkedCash`;
  a pack swaps the look). Same quantity component, so stacks split and merge like cash, but only with
  other marked notes (merging and counting match the exact prefab).
- ✅ `Add(target, amount, reason[, dropOverflow])`, `Count(target)`, `Take(target, amount[, reason])`,
  the `LF_MoneyUtils` shape. `LF_MoneyUtils.GetCash`, shops, the ATM and hand-overs never count or
  spend them. Nothing is published as `LF_OnMoneyMoved`; each change logs `[LFDebug:Laundering]`
  with its reason. A slot with no usable prefab makes `Add` log `LF_Debug.Error("Laundering", ...)`
  and return 0 (`GetLastError`).
- ✅ No save payload: they are items in inventories and persist like any other item.
- ✅ Dirty money is money: with `persistence.keepCash` off, a wipe strips marked notes from every slot it
  strips cash from (`LF_PersistenceKeep.StripMarkedNotes`: carried, civilian stash, duty kits, car cargo).
  Test: `LF_Test_MarkedCashWipe` (LOGIC).
- ⚠️ Looks the same as cash for now (same mesh, only the name differs); a band or tint is a follow-up.
- Test: `LF_Test_MarkedCash` (WORLD).

### Dirty payouts — `Feature/Laundering/LF_LaunderConfig.c` (#2851)
- ✅ `LF_LaunderConfig` (`Configs/Crime/LF_Launder.conf`, the feature's one config; a pack overrides the
  file) carries `m_bCrimePaysMarked`. Core ships it **off**: every payout is exactly today's cash.
- ✅ Switch on: a till robbery (`LF_RobAction.PayCash`) pays the same amount as marked notes and no cash
  (reason `robbery`; a short add is taken back and the robbery refused, like the cash path), and a bank
  vault's money bags (`LF_Bank.WriteWorldBagCash`: the vault bag, the heist spill, the bag re-filled on
  load) hold marked notes (`LF_Bank.ReadWorldBagMarked`). A bag's marked stack never reads as cash.
- ✅ Chop shop and fence payouts stay clean cash (the fence is already the dirty exit for goods).
- Test: `LF_Test_DirtyPayout` (WORLD).
