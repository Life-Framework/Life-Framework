# Catalogue

## Intent

Who can buy which weapon, piece of clothing or equipment, where, and for how much is a server's
identity, so it is data: one catalogue of items by id, and outlets (shop counters) that each sell some
of them behind a gate. A refusal names what the buyer lacks ("Not without a Firearm permit.").

## Interaction pattern

In-world: the buyer walks up to a counter (`LF_ShopComponent` with `m_sOutletId`) and uses its F-key
action, which opens the order slip every shop uses (`LF_ShopPriceList`). The counter is the purchase
point; the slip is where the gate's answer is printed.

## Item catalogue and outlets (#988)

- **Player sees:** at an outlet counter, the outlet's items at catalogue prices. Ordering one the gate
  refuses prints the reason with the missing thing named: a licence (`#LF-Shop_RefuseNeedsLicence`),
  a job (`...NeedsJob`), a rank (`...NeedsRank`) or an organisation right (`...NeedsOrgRight`).
- **Owner and side:** the server decides every sale: `LF_ShopComponent.Buy` asks
  `LF_ItemCatalogue.Check(offer, LF_EntityBuyerFacts.Of(buyer), missing)` and replies with the refusal
  and `missing` (a localization key) over `LF_CharacterShopComponent.RpcDo_TransactionResult`. A client
  draws the price list from the same catalogue, which the config stream brings up to the server's values.
- **Data:** `Configs/Catalogue/LF_ItemCatalogue.conf` (`LF_ItemCatalogueConfig`), each pack overlay of
  that class, the owner's `catalogue` section, then on a client the stream. Records merge by id
  (`items` by item id, `offers` by `outlet/item`) through the shared override model
  (`docs/config/override-model.md`, #962): add, replace, patch or remove; with no verb an existing id
  changes only the fields a layer writes and a new id is added. `removed: true` hides a record and a
  later layer can set it back. Nothing is persisted.
- **Item:** id, prefab, category (`weapon`, `ammo`, `clothing`, `equipment`, `tool`), price, sell price,
  legality (`legal`, `licensed`, `restricted`, `illegal`), the licence that makes carrying it lawful,
  and the clothing flags `concealsFace` and `concealsBuild`.
- **Offer (gate):** outlet id, item id, and any of: a licence (`LF_ELicenseType` name), a job with a
  minimum rank (`POLICE` is an on-duty officer and the rank is the police rank index; any other job id
  is a job level, `LF_JobLevels`), an organisation right (`LF_EOrganizationPermission` name). Every gate
  that is set must pass; the refusal names the first that does not (licence, job, rank, right).
- **Legality:** `LF_ItemCatalogue.OffenceFor(item, carrier)` gives the crime catalog id for carrying an
  item: `#LF-Offence_UnlicensedItem` (licensed, no licence), `#LF-Offence_RestrictedItem` (restricted,
  no licence), `#LF-Offence_IllegalItem` (illegal), or nothing. The numbers live in
  `Configs/Crime/LF_CrimeCatalog.conf`. Police search and seizure do not read it yet (follow-up).
- **Clothing:** `LF_ItemCatalogue.Concealment(config, worn, face, build)` says what a set of worn
  prefabs hides from a witness. The suspect-look capture (#889) does not read it yet (follow-up).
- **Load checks** (`LF_ItemCatalogueConfig.Check`, after every layer): an item with no prefab, a prefab
  that does not load, or a DLC-owned prefab (`LF_ItemCatalogue.DLC_PATH_FRAGMENTS`) is removed; a sell
  price above the price is lowered to the price; an offer for an item the catalogue lacks is removed.
  Each logs `LF_Debug.Error` on `[LFDebug:Catalogue]`.
- **Validate:** `tools/validation/validate-item-catalogue.mjs` reads every addon `.conf` whose root is
  `LF_ItemCatalogueConfig`: each prefab is an addon resource with that `.meta` GUID or a base game prefab
  (the installed game's resource database; CI skips that half), none is in
  `tools/validation/dlc-owned-prefabs.txt` (made with `--refresh-dlc` from the installed Rally DLC), no
  sell price is above its price, every offer names a listed item.
- **Core defaults** (one example per outlet type): `gun_counter` sells the M9 behind `FIREARM_PERMIT`,
  `clothing_rack` sells a green raincoat (hides the build), `police_armory` sells the M16A2 to an
  on-duty officer of rank 1 (senior) or more. The annex has one gun counter
  (`Prefabs/Props/Commercial/LF_GunCounter.et`, `Annex_GunCounter`); the armory and the rack are
  config only.
- **Testing a licence:** `/licence FIREARM_PERMIT` in chat grants the caller one licence by name
  (`LF_LicenceGrantCommand.c`; administrators, dev worlds and harness builds only).
- **Proof:** `catalogue/outlets` (LOGIC, `LF_Test_ItemCatalogue`), `catalogue/gun-counter` (WORLD, `LF_Test_GunCounter`: the annex counter refuses without the permit and sells with it), `item-catalogue-selftest.mjs`.

## Not yet (follow-ups of #988)

- Stock and restock per offer: #1064.
- The black market, back room, fence and the `Prefabs/Weapons/` price rule onto outlets: #1065.
- The police armory issuing from its outlet by rank: #1066.
- Police search and seizure reading the legality flags: #1067.
- The suspect look reading the conceal flags: #1068.
- A licence desk for the firearm permit: #1069.
