# Outposts

### `LF_Outpost` — `Feature/Outposts/LF_Outpost.c` (#4217, 2026-11-23)
- ⚠️ A rebel outpost is a safe zone: `LF_OutpostComponent` registers a SAFE circle (radius from `Configs/Outposts/LF_Outposts.conf`, 60 m by default) and the place under kind `outpost`. Nobody fights inside; police may walk in. A trader inside refuses an on-duty officer with a toast ("We don't deal with the law here. Take the badge off and come back."); off-duty police are served. The rule is `LF_Outpost.RefusesService(customer, traderPos)`; `m_bTradersRefuseOnDutyPolice` in the conf turns it off. Merged, in-dev: the rule, zone and words are tested; the trader sale and shop buy paths do not call it yet and no outpost prefab is placed (later cards). Test `outposts/rules` (LOGIC).
