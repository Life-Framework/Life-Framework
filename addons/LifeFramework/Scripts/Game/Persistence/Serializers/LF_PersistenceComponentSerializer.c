//------------------------------------------------------------------------------------------------
//! Persists the game-mode persistence store (LF_PersistenceComponent): player accounts, bank
//! accounts, survival stats, organizations, hideout ownership, owned vehicle records, and serving
//! jail sentences, and carried inventories.
//!
//! BINDING. Listed in the ComponentSerializers block of Configs/Systems/Persistence/LifeFramework.conf,
//! matching the game-mode entity that carries LF_PersistenceComponent.
//!
//! WHERE THE DATA LIVES. Account, bank, and organization records are pulled from and applied to
//! the manager singletons directly - LF_PlayerAccountManager / LF_ATMManager /
//! LF_OrganizationManager own those caches, not this component. Only survival stats are read
//! from and written into the component's own map (keyed by character persistence id).
//! Hideout records live in LF_Hideout. Vehicle records live in LF_VehicleOwnership, keyed by
//! the lock identifier. Jail sentences live in LF_Jail, keyed by the player uid. Carried
//! inventories live in LF_StorageSnapshot, keyed by character persistence id and slot name. Placed
//! storage (LF_PersistentStorageComponent, the hideout stash) rides the same store under its key.
//! inventories live in LF_StorageSnapshot, keyed by character persistence id and slot name.
//! Progression (career, police rank and duty seconds, benefit cooldown, level, licences, quest
//! turn-ins) lives in LF_Progression and its owners, keyed by the player uid.
//! "Player uid" in this list means the character record key (#331, LF_Utils.GetPlayerUID): slot 1 of
//! an account keeps the account uid, later slots their own id. The keys and every character's
//! wanted level are the "characters" payload, in LF_PlayerAccountManager.
//! Introductions (who knows whose name) live in LF_Introductions, keyed by player uid pairs.
//! Per-item state of a snapshot (magazine ammo) lives beside its snapshot in LF_StorageSnapshot.
//! Wanted reports (the description recorded at each crime and its identified flag) live in
//! LF_WantedReports. Delivery-job sign-ups live in LF_CivilianJob, keyed by the player uid.
//! Police tickets (amount, deadline clock, paid flag) live in LF_FineTickets.
//! The incident log (deaths, sampled damage, cash moves; server only) lives in LF_IncidentLog.
//! The money ledger (every cash and bank change, trimmed by LF_LedgerConfig) lives in LF_Ledger.
//!
//! FORMAT. Binary save contexts are POSITIONAL: write order must equal read order. Version is
//! written first. Accounts and bank are the prefix on every save, including version 1.
//! RegisterPersistencePayloads is the only order after that prefix: survival, then organizations,
//! hideout, vehicles, jail, carried, progression, introductions, itemstate, wanted, jobsignups, tickets, incidents, ledger, crimerecord, fenceserials, incidentflags, evidencelocker, bills, chainstations, storestock, itemserials, farmplots, characters, storeunpaid, playtime, orgranks, joblevels, wantedlooks. The written version is that list's length (29). Serialize walks the
//! whole list. Deserialize walks the same list and stops at the version stored in the save.
//! A version 1 save stops after survival (no organization payload). A version 2 save includes
//! organizations and has no hideouts. A version 3 save includes hideouts and has no vehicles.
//! A version 4 save includes vehicles and has no jail sentences.
//! A version 5 save includes jail sentences and has no carried inventories.
//! A version 6 save includes carried inventories and has no progression.
//! A version 7 save includes progression and has no introductions.
//! A version 8 save includes introductions and has no per-item state (magazine ammo).
//! A version 9 save includes per-item state and has no wanted reports.
//! A version 10 save includes wanted reports and has no job sign-ups.
//! A version 11 save includes job sign-ups and has no police tickets.
//! A version 12 save includes police tickets and has no incident log.
//! A version 13 save includes the incident log and has no money ledger.
//! A version 14 save includes the money ledger and has no crime record.
//! A version 15 save includes the crime record and has no fence serial book.
//! A version 16 save includes the fence serial book and has no incident flags.
//! A version 17 save includes the incident flags and has no evidence locker.
//! A version 18 save includes the evidence locker and has no treatment bills.
//! A version 19 save includes the treatment bills and has no chain stations (half-finished batches).
//! A version 20 save includes the chain stations and has no store stock (#202).
//! A version 21 save includes the store stock and has no item serials (#470).
//! A version 22 save includes the item serials and has no farm plots (#614).
//! A version 23 save includes the farm plots and has no character slots (#331): each account's one
//! character loads as slot 1, keyed by the account uid its records already use.
//! A version 24 save includes the character slots and has no unpaid store tags (#479): goods taken off a
//! shelf before that restart come back untagged.
//! A version 25 save includes the unpaid store tags and has no playtime (#692): every account's
//! playtime starts at zero.
//! A version 26 save includes the playtime and has no organization rank rows (#750a): its organization
//! records are record version 1, so LF_OrganizationManager.ApplyAll turns each rank's four bools into
//! a rights mask and adds the officer rung; members keep their rank.
//! A version 27 save includes the organization ranks and has no job XP rows (#749): the
//! "progression" payload seeds LF_JobLevels from its records' single level (onto CIVILIAN) and
//! delivery career (onto DELIVERY), and that is the migration; every other job starts at 0.
//! A version 28 save includes the job XP rows and has no wanted looks (#758): its reports load with
//! no look, so their posters on the wanted board's face show an empty photo frame until the
//! suspect's next crime.
//! The next system adds one Insert at the end of the list and raises the written version by one.
//! Do not insert earlier and do not reorder: existing saves are positional.
//!
//! IDEMPOTENT ON A LIVE SESSION. Deserialize also runs when saved data is re-applied to a running
//! session. The manager ApplyAll() implementations are plain replacement of cached state, and the
//! survival map is cleared before being rebuilt, so a second pass produces exactly the same end
//! state as the first. A payload the stored version does not contain is left untouched.
//------------------------------------------------------------------------------------------------
class LF_PersistenceComponentSerializer : ScriptedComponentSerializer
{
	//! True once DeserializeStore ran this boot. A boot that continues a save and reaches ACTIVE
	//! with this still false has a save written before the pre-release LF_ rename (the engine matches
	//! the component key LF_PersistenceComponent:<id> literally), see docs/persistence.md "Save reset".
	protected static bool s_bStoreLoaded;

	//------------------------------------------------------------------------------------------------
	//! eturn True when a save payload was read into the stores this boot.
	static bool WasStoreLoaded()
	{
		return s_bStoreLoaded;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return LF_PersistenceComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! The only payload order. Index 0 is version 1. Written version equals this length.
	//! Append the next system at the end; do not insert or reorder.
	//! \param[out] order Empty array filled with the payload names in save order.
	protected static void RegisterPersistencePayloads(notnull array<string> order)
	{
		order.Insert("survival");
		order.Insert("organizations");
		order.Insert("hideout");
		order.Insert("vehicles");
		order.Insert("jail");
		order.Insert("carried");
		order.Insert("progression");
		order.Insert("introductions");
		order.Insert("itemstate");
		order.Insert("wanted");
		order.Insert("jobsignups");
		order.Insert("tickets");
		order.Insert("incidents");
		order.Insert("ledger");
		order.Insert("crimerecord");
		order.Insert("fenceserials");
		order.Insert("incidentflags");
		order.Insert("evidencelocker");
		order.Insert("bills");
		order.Insert("chainstations");
		order.Insert("storestock");
		order.Insert("itemserials");
		order.Insert("farmplots");
		order.Insert("characters");
		order.Insert("storeunpaid");
		order.Insert("playtime");
		order.Insert("orgranks");
		order.Insert("joblevels");
		order.Insert("wantedlooks");
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the account, bank and survival records into the save context.
	//! \param[in] owner The game-mode entity owning the persistence component.
	//! \param[in] component The persistence component being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an LF_PersistenceComponent.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		LF_PersistenceComponent persistence = LF_PersistenceComponent.Cast(component);
		if (!persistence)
			return ESerializeResult.ERROR;

#ifdef LF_E2E_LEGACY_SAVE
		// tools\cli wt e2e <slug> --legacy-save: the save boot writes the last format before #331,
		// so the reload boot proves an old save loads into character slot 1.
		LF_Debug.Info("Persistence", "LF_E2E_LEGACY_SAVE: writing the save at version 23");
		return SerializeStoreAsVersion(persistence, context, 23);
#else
		return SerializeStore(persistence, context);
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! The whole store write. Public so a test drives the real write path into its own context.
	//! \param[in] persistence The game-mode persistence component.
	//! \param[in] context Save context to write into.
	//! \return OK once every payload is written.
	static ESerializeResult SerializeStore(notnull LF_PersistenceComponent persistence, notnull SaveContext context)
	{
		array<string> order = new array<string>();
		RegisterPersistencePayloads(order);
		context.WriteValue("version", 29);
		WritePrefix(context);

		foreach (string payload : order)
		{
			WritePersistencePayload(persistence, context, payload);
		}

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the store in the shape an older build wrote: that version, the prefix, and only the
	//! payloads that version has. Tests and the legacy-save e2e use it to make a real old save; the
	//! game always saves through SerializeStore.
	//! \param[in] version Save version to write, 1 to the current one.
	static ESerializeResult SerializeStoreAsVersion(notnull LF_PersistenceComponent persistence, notnull SaveContext context, int version)
	{
		array<string> order = new array<string>();
		RegisterPersistencePayloads(order);
		int written = Math.ClampInt(version, 1, order.Count());
		context.WriteValue("version", written);
		WritePrefix(context);

		for (int index = 0; index < written; index++)
		{
			WritePersistencePayload(persistence, context, order.Get(index), written);
		}

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! The account and bank records every save version starts with.
	protected static void WritePrefix(notnull SaveContext context)
	{
		// Accounts live in the player account manager singleton. A null result (no manager / no
		// data) is written as an empty array so the positional read order stays intact.
		array<ref LF_PlayerAccountRecord> accountRecords = LF_PlayerAccountManager.ExportAll();
		if (!accountRecords)
			accountRecords = new array<ref LF_PlayerAccountRecord>();
		context.Write(accountRecords);

		// Bank accounts live in the ATM manager singleton.
		array<ref LF_BankAccountRecord> bankRecords = LF_ATMManager.ExportAll();
		if (!bankRecords)
			bankRecords = new array<ref LF_BankAccountRecord>();
		context.Write(bankRecords);
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the records back and hands them to the managers / the component to apply.
	//! \param[in] owner The game-mode entity owning the persistence component.
	//! \param[in] component The persistence component being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		LF_PersistenceComponent persistence = LF_PersistenceComponent.Cast(component);
		if (!persistence)
			return false;

		return DeserializeStore(persistence, context);
	}

	//------------------------------------------------------------------------------------------------
	//! The whole store read. Public so a test drives the real read path from its own context.
	//! \param[in] persistence The game-mode persistence component.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	static bool DeserializeStore(notnull LF_PersistenceComponent persistence, notnull LoadContext context)
	{
		s_bStoreLoaded = true;
		array<string> order = new array<string>();
		RegisterPersistencePayloads(order);

		// No version means this component has no payload in the stored record - leave the live
		// session alone.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
		{
			LF_Debug.Warn("Persistence", "load skipped: no version header (save predates the serializer)");
			return true;
		}

		array<ref LF_PlayerAccountRecord> accountRecords = new array<ref LF_PlayerAccountRecord>();
		if (context.Read(accountRecords))
		{
			LF_PlayerAccountManager.ApplyAll(accountRecords);
			LF_Debug.Log("Persistence", string.Format("applied %1 account record(s)", accountRecords.Count()));
		}
		else
		{
			LF_Debug.Warn("Persistence", "account records failed to read, leaving live accounts untouched");
		}

		array<ref LF_BankAccountRecord> bankRecords = new array<ref LF_BankAccountRecord>();
		if (context.Read(bankRecords))
		{
			LF_ATMManager.ApplyAll(bankRecords);
			LF_Debug.Log("Persistence", string.Format("applied %1 bank account record(s)", bankRecords.Count()));
		}
		else
		{
			LF_Debug.Warn("Persistence", "bank records failed to read, leaving live balances untouched");
		}

		for (int index = 0; index < order.Count(); index++)
		{
			if (index >= version)
				break;

			ReadPersistencePayload(persistence, context, order.Get(index));
		}

		// After every payload: a wanted report is kept only while the character it names is wanted,
		// and the "characters" payload restores those levels after "wanted" loads (#331).
		if (version > order.Find("wanted"))
			LF_WantedReports.PruneCleared();

		// A save from before #331 has no character slots. ApplyAll already keyed each account's one
		// character by the account uid (slot 1), which is what that save's records use.
		// A save from before #749 has no job XP rows: "progression" seeded them from its single
		// level and delivery career.
		if (version > order.Find("progression") && version <= order.Find("joblevels"))
			LF_Debug.Info("Persistence", string.Format("save version %1 has no job xp rows: job levels migrated from the single level (CIVILIAN) and the delivery career (DELIVERY), %2 job xp row(s)", version, LF_JobLevels.ExportAll().Count()));

		if (version <= order.Find("characters"))
			LF_Debug.Info("Persistence", string.Format("save version %1 has no character slots: %2 account(s) load their character into slot 1, keyed by the account uid", version, LF_PlayerAccountManager.CountAccountsWithCharacters()));

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes one registered payload. The list, not this function, decides the order.
	//! \param[in] persistence Persistence component that owns survival stats.
	//! \param[in] context Save context to write into.
	//! \param[in] payload Name from RegisterPersistencePayloads.
	//! \param[in] saveVersion The version an older-format save is written at (SerializeStoreAsVersion),
	//! 0 for the current one. A payload whose records changed meaning writes the old meaning.
	static void WritePersistencePayload(notnull LF_PersistenceComponent persistence, notnull SaveContext context, string payload, int saveVersion = 0)
	{
		if (payload == "survival")
		{
			array<ref LF_SurvivalStatsRecord> survivalRecords = new array<ref LF_SurvivalStatsRecord>();
			map<string, ref LF_SurvivalStats> survivalMap = persistence.GetSurvivalStatsMap();
			if (survivalMap)
			{
				foreach (string characterId, LF_SurvivalStats stats : survivalMap)
				{
					if (!stats)
						continue;

					LF_SurvivalStatsRecord record = LF_SurvivalStatsRecord.Create(
						characterId,
						stats.GetHunger(),
						stats.GetThirst(),
						LF_SurvivalStatsRecord.RETIRED_HEALTH
					);
					survivalRecords.Insert(record);
				}
			}
			context.Write(survivalRecords);
		}
		else if (payload == "organizations")
		{
			array<ref LF_OrganizationRecord> orgRecords = LF_OrganizationManager.ExportAll();
			if (!orgRecords)
				orgRecords = new array<ref LF_OrganizationRecord>();
			// A save older than the "orgranks" payload has record version 1 (#750a): its ranks are
			// only the four bools, which is what a build before the rights mask wrote.
			array<string> order = new array<string>();
			RegisterPersistencePayloads(order);
			if (saveVersion > 0 && saveVersion <= order.Find("orgranks"))
			{
				foreach (LF_OrganizationRecord legacyRecord : orgRecords)
				{
					if (legacyRecord)
						legacyRecord.m_iVersion = 1;
				}
			}
			context.Write(orgRecords);
		}
		else if (payload == "hideout")
		{
			array<ref LF_HideoutRecord> hideoutRecords = LF_Hideout.ExportAll();
			if (!hideoutRecords)
				hideoutRecords = new array<ref LF_HideoutRecord>();
			context.Write(hideoutRecords);
		}
		else if (payload == "vehicles")
		{
			array<ref LF_VehicleRecord> vehicleRecords = LF_VehicleOwnership.ExportAll();
			if (!vehicleRecords)
				vehicleRecords = new array<ref LF_VehicleRecord>();
			context.Write(vehicleRecords);
		}
		else if (payload == "jail")
		{
			array<ref LF_JailRecord> jailRecords = LF_Jail.ExportAll();
			if (!jailRecords)
				jailRecords = new array<ref LF_JailRecord>();
			context.Write(jailRecords);
		}
		else if (payload == "carried")
		{
			// Every save point (autosave, manual, shutdown) refreshes living players first.
			int captured = LF_StorageSnapshot.CaptureAllPlayers();
			int placed = LF_PersistentStorageComponent.CaptureAllPlaced();
			array<ref LF_StorageSnapshotRecord> carriedRecords = LF_StorageSnapshot.ExportAll();
			if (!carriedRecords)
				carriedRecords = new array<ref LF_StorageSnapshotRecord>();
			context.Write(carriedRecords);
			LF_Debug.Log("Persistence", string.Format("saved %1 carried snapshot(s), %2 captured from live players, %3 from placed storage", carriedRecords.Count(), captured, placed));
		}
		else if (payload == "progression")
		{
			array<ref LF_ProgressionRecord> progressionRecords = LF_Progression.ExportAll();
			if (!progressionRecords)
				progressionRecords = new array<ref LF_ProgressionRecord>();
			context.Write(progressionRecords);
			LF_Debug.Log("Persistence", string.Format("saved %1 progression record(s)", progressionRecords.Count()));
		}
		else if (payload == "introductions")
		{
			array<ref LF_IntroductionRecord> introductionRecords = LF_Introductions.ExportAll();
			if (!introductionRecords)
				introductionRecords = new array<ref LF_IntroductionRecord>();
			context.Write(introductionRecords);
			LF_Debug.Log("Persistence", string.Format("saved %1 introduction(s)", introductionRecords.Count()));
		}
		else if (payload == "itemstate")
		{
			// Written after carried: the carried write captured live players, so this matches it.
			array<ref LF_StorageItemStateRecord> stateRecords = LF_StorageSnapshot.ExportItemState();
			if (!stateRecords)
				stateRecords = new array<ref LF_StorageItemStateRecord>();
			context.Write(stateRecords);
			LF_Debug.Log("Persistence", string.Format("saved per-item state of %1 snapshot(s)", stateRecords.Count()));
		}
		else if (payload == "wanted")
		{
			array<ref LF_WantedReportRecord> wantedRecords = LF_WantedReports.ExportAll();
			if (!wantedRecords)
				wantedRecords = new array<ref LF_WantedReportRecord>();
			context.Write(wantedRecords);
			LF_Debug.Log("Persistence", string.Format("saved %1 wanted report(s)", wantedRecords.Count()));
		}
		else if (payload == "jobsignups")
		{
			array<ref LF_JobSignUpRecord> signUpRecords = LF_CivilianJob.ExportSignUps();
			if (!signUpRecords)
				signUpRecords = new array<ref LF_JobSignUpRecord>();
			context.Write(signUpRecords);
			LF_Debug.Log("Persistence", string.Format("saved %1 job sign-up(s)", signUpRecords.Count()));
		}
		else if (payload == "tickets")
		{
			array<ref LF_FineTicketRecord> ticketRecords = LF_FineTickets.ExportAll();
			if (!ticketRecords)
				ticketRecords = new array<ref LF_FineTicketRecord>();
			context.Write(ticketRecords);
			LF_Debug.Log("Persistence", string.Format("saved %1 police ticket(s)", ticketRecords.Count()));
		}
		else if (payload == "incidents")
		{
			array<ref LF_IncidentRecord> incidentRecords = LF_IncidentLog.ExportAll();
			context.Write(incidentRecords);
			LF_Debug.Log("Persistence", string.Format("saved %1 incident(s)", incidentRecords.Count()));
		}
		else if (payload == "ledger")
		{
			array<ref LF_LedgerEntry> ledgerEntries = LF_Ledger.ExportAll();
			context.Write(ledgerEntries);
			LF_Debug.Log("Persistence", string.Format("saved %1 ledger entr(ies)", ledgerEntries.Count()));
		}
		else if (payload == "crimerecord")
		{
			array<ref LF_CrimeRecordRow> crimeRows = LF_CrimeRecord.ExportAll();
			context.Write(crimeRows);
			LF_Debug.Log("Persistence", string.Format("saved %1 crime record row(s)", crimeRows.Count()));
		}
		else if (payload == "fenceserials")
		{
			array<ref LF_FenceSerialRow> serialRows = LF_FenceSerials.ExportAll();
			context.Write(serialRows);
			LF_Debug.Log("Persistence", string.Format("saved %1 fence serial row(s)", serialRows.Count()));
		}
		else if (payload == "incidentflags")
		{
			array<ref LF_IncidentFlagRow> flagRows = LF_IncidentFlags.ExportAll();
			context.Write(flagRows);
			LF_Debug.Log("Persistence", string.Format("saved %1 incident flag(s)", flagRows.Count()));
		}
		else if (payload == "evidencelocker")
		{
			array<ref LF_EvidenceLockerRow> lockerRows = LF_EvidenceLocker.ExportAll();
			context.Write(lockerRows);
			LF_Debug.Log("Persistence", string.Format("saved %1 evidence locker row(s)", lockerRows.Count()));
		}
		else if (payload == "bills")
		{
			array<ref LF_TreatmentBillRecord> billRecords = LF_TreatmentBills.ExportAll();
			context.Write(billRecords);
			LF_Debug.Log("Persistence", string.Format("saved %1 treatment bill(s)", billRecords.Count()));
		}
		else if (payload == "chainstations")
		{
			array<ref LF_ChainStationRow> stationRows = LF_ChainStations.ExportAll();
			context.Write(stationRows);
			LF_Debug.Log("Persistence", string.Format("saved %1 chain station batch(es)", stationRows.Count()));
		}
		else if (payload == "storestock")
		{
			array<ref LF_StoreStockRow> stockRows = LF_Stores.ExportAll();
			context.Write(stockRows);
			LF_Debug.Log("Persistence", string.Format("saved %1 store stock row(s)", stockRows.Count()));
		}
		else if (payload == "itemserials")
		{
			// Written after carried, like itemstate: the snapshots hold the live items' serials.
			array<ref LF_StorageItemSerialRecord> serialRecords = LF_StorageSnapshot.ExportItemSerials();
			context.Write(serialRecords);
			LF_Debug.Log("Persistence", string.Format("saved item serials of %1 snapshot(s)", serialRecords.Count()));
		}
		else if (payload == "farmplots")
		{
			array<ref LF_FarmPlotRow> plotRows = LF_FarmPlots.ExportAll();
			context.Write(plotRows);
			LF_Debug.Log("Persistence", string.Format("saved %1 planted farm plot(s)", plotRows.Count()));
		}
		else if (payload == "characters")
		{
			array<ref LF_CharacterSlotRecord> slotRows = LF_PlayerAccountManager.ExportCharacterSlots();
			context.Write(slotRows);
			LF_Debug.Log("Persistence", string.Format("saved %1 character slot(s)", slotRows.Count()));
		}
		else if (payload == "storeunpaid")
		{
			array<ref LF_StoreUnpaidRow> unpaidRows = LF_Stores.ExportUnpaid();
			context.Write(unpaidRows);
			LF_Debug.Log("Persistence", string.Format("saved %1 unpaid store tag(s)", unpaidRows.Count()));
		}
		else if (payload == "playtime")
		{
			// ExportAll credits the online clocks first, so the save holds the time up to now.
			array<ref LF_PlaytimeRecord> playtimeRows = LF_Playtime.ExportAll();
			context.Write(playtimeRows);
			LF_Debug.Log("Persistence", string.Format("saved %1 playtime row(s)", playtimeRows.Count()));
		}
		else if (payload == "orgranks")
		{
			array<ref LF_OrganizationRankRow> rankRows = LF_OrganizationManager.ExportRanks();
			context.Write(rankRows);
			LF_Debug.Log("Persistence", string.Format("saved %1 organization rank row(s)", rankRows.Count()));
		}
		else if (payload == "joblevels")
		{
			array<ref LF_JobXpRow> jobXpRows = LF_JobLevels.ExportAll();
			context.Write(jobXpRows);
			LF_Debug.Log("Persistence", string.Format("saved %1 job xp row(s)", jobXpRows.Count()));
		}
		else if (payload == "wantedlooks")
		{
			array<ref LF_WantedLookRow> lookRows = LF_WantedReports.ExportLooks();
			context.Write(lookRows);
			LF_Debug.Log("Persistence", string.Format("saved %1 wanted look(s)", lookRows.Count()));
		}
		else
		{
			LF_Debug.Error("Persistence", string.Format("save skipped unknown payload '%1'", payload));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Reads one registered payload and stops the caller from applying anything past the stored version.
	//! \param[in] persistence Persistence component that owns survival stats.
	//! \param[in] context Load context to read from.
	//! \param[in] payload Name from RegisterPersistencePayloads.
	static void ReadPersistencePayload(notnull LF_PersistenceComponent persistence, notnull LoadContext context, string payload)
	{
		if (payload == "survival")
		{
			// Survival stats are owned by the component. Clear before rebuilding so re-applying to a
			// live session yields exactly the saved map (idempotent). Only clear when the read
			// succeeded, so a corrupt payload does not wipe live stats.
			array<ref LF_SurvivalStatsRecord> survivalRecords = new array<ref LF_SurvivalStatsRecord>();
			if (context.Read(survivalRecords))
			{
				persistence.ClearSurvivalStats();
				foreach (LF_SurvivalStatsRecord record : survivalRecords)
				{
					if (!record)
						continue;

					LF_SurvivalStats stats = LF_SurvivalStats.Create(record.m_sPersistentId);
					stats.SetHunger(record.m_fHunger);
					stats.SetThirst(record.m_fThirst);
					persistence.SetSurvivalStats(record.m_sPersistentId, stats);
				}
				LF_Debug.Log("Persistence", string.Format("applied %1 survival record(s)", survivalRecords.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "survival records failed to read, leaving live stats untouched");
			}
		}
		else if (payload == "organizations")
		{
			// A version 1 save never reaches this payload, so a live fund the save never
			// knew about is left alone.
			array<ref LF_OrganizationRecord> orgRecords = new array<ref LF_OrganizationRecord>();
			if (context.Read(orgRecords))
			{
				LF_OrganizationManager.ApplyAll(orgRecords);
				LF_Debug.Log("Persistence", string.Format("applied %1 organization record(s)", orgRecords.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "organization records failed to read, leaving live organizations untouched");
			}
		}
		else if (payload == "hideout")
		{
			array<ref LF_HideoutRecord> hideoutRecords = new array<ref LF_HideoutRecord>();
			if (context.Read(hideoutRecords))
			{
				LF_Hideout.ApplyAll(hideoutRecords);
				LF_Debug.Log("Persistence", string.Format("applied %1 hideout record(s)", hideoutRecords.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "hideout records failed to read, leaving live hideouts untouched");
			}
		}
		else if (payload == "vehicles")
		{
			array<ref LF_VehicleRecord> vehicleRecords = new array<ref LF_VehicleRecord>();
			if (context.Read(vehicleRecords))
			{
				LF_VehicleOwnership.ApplyAll(vehicleRecords);
				int impounded = LF_VehicleOwnership.ImpoundStreetVehicles();
				LF_Debug.Log("Persistence", string.Format("applied %1 vehicle record(s), %2 at the impound lot", vehicleRecords.Count(), impounded));
			}
			else
			{
				LF_Debug.Warn("Persistence", "vehicle records failed to read, leaving live vehicles untouched");
			}
		}
		else if (payload == "jail")
		{
			// A version 1-4 save never reaches this payload, so live sentences are left alone.
			array<ref LF_JailRecord> jailRecords = new array<ref LF_JailRecord>();
			if (context.Read(jailRecords))
			{
				LF_Jail.ApplyAll(jailRecords);
				LF_Debug.Log("Persistence", string.Format("applied %1 jail record(s)", jailRecords.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "jail records failed to read, leaving live sentences untouched");
			}
		}
		else if (payload == "carried")
		{
			// A version 1-5 save never reaches this payload, so live snapshots are left alone.
			array<ref LF_StorageSnapshotRecord> carriedRecords = new array<ref LF_StorageSnapshotRecord>();
			if (context.Read(carriedRecords))
			{
				LF_StorageSnapshot.ApplyAll(carriedRecords);
				LF_Debug.Log("Persistence", string.Format("applied %1 carried snapshot(s)", carriedRecords.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "carried snapshots failed to read, leaving live snapshots untouched");
			}
		}
		else if (payload == "progression")
		{
			// A version 1-6 save never reaches this payload, so live progression is left alone.
			array<ref LF_ProgressionRecord> progressionRecords = new array<ref LF_ProgressionRecord>();
			if (context.Read(progressionRecords))
			{
				LF_Progression.ApplyAll(progressionRecords);
				LF_Debug.Log("Persistence", string.Format("applied %1 progression record(s)", progressionRecords.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "progression records failed to read, leaving live progression untouched");
			}
		}
		else if (payload == "introductions")
		{
			// A version 1-7 save never reaches this payload, so live introductions are left alone.
			array<ref LF_IntroductionRecord> introductionRecords = new array<ref LF_IntroductionRecord>();
			if (context.Read(introductionRecords))
			{
				LF_Introductions.ApplyAll(introductionRecords);
				LF_Debug.Log("Persistence", string.Format("applied %1 introduction record(s)", introductionRecords.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "introduction records failed to read, leaving live introductions untouched");
			}
		}
		else if (payload == "itemstate")
		{
			// A version 1-8 save never reaches this payload: restored magazines keep their prefab ammo.
			array<ref LF_StorageItemStateRecord> stateRecords = new array<ref LF_StorageItemStateRecord>();
			if (context.Read(stateRecords))
			{
				int matched = LF_StorageSnapshot.ApplyItemState(stateRecords);
				LF_Debug.Log("Persistence", string.Format("applied per-item state to %1 of %2 snapshot(s)", matched, stateRecords.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "per-item state failed to read, restored magazines keep their prefab ammo");
			}
		}
		else if (payload == "wanted")
		{
			// A version 1-9 save never reaches this payload, so live reports are left alone.
			array<ref LF_WantedReportRecord> wantedRecords = new array<ref LF_WantedReportRecord>();
			if (context.Read(wantedRecords))
			{
				LF_WantedReports.ApplyAll(wantedRecords);
				LF_Debug.Log("Persistence", string.Format("applied %1 wanted report(s)", wantedRecords.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "wanted reports failed to read, leaving live reports untouched");
			}
		}
		else if (payload == "jobsignups")
		{
			// A version 1-10 save never reaches this payload, so live sign-ups are left alone.
			array<ref LF_JobSignUpRecord> signUpRecords = new array<ref LF_JobSignUpRecord>();
			if (context.Read(signUpRecords))
			{
				LF_CivilianJob.ApplySignUps(signUpRecords);
				LF_Debug.Log("Persistence", string.Format("applied %1 job sign-up record(s)", signUpRecords.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "job sign-up records failed to read, leaving live sign-ups untouched");
			}
		}
		else if (payload == "tickets")
		{
			// A version 1-11 save never reaches this payload, so live tickets are left alone.
			array<ref LF_FineTicketRecord> ticketRecords = new array<ref LF_FineTicketRecord>();
			if (context.Read(ticketRecords))
			{
				LF_FineTickets.ApplyAll(ticketRecords);
				LF_Debug.Log("Persistence", string.Format("applied %1 police ticket(s)", ticketRecords.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "police tickets failed to read, leaving live tickets untouched");
			}
		}
		else if (payload == "incidents")
		{
			// A version 1-12 save never reaches this payload, so the live log is left alone.
			array<ref LF_IncidentRecord> incidentRecords = new array<ref LF_IncidentRecord>();
			if (context.Read(incidentRecords))
			{
				LF_IncidentLog.ApplyAll(incidentRecords);
				LF_Debug.Log("Persistence", string.Format("applied %1 incident(s)", incidentRecords.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "incident log failed to read, leaving the live log untouched");
			}
		}
		else if (payload == "ledger")
		{
			// A version 1-13 save never reaches this payload, so the live ledger is left alone.
			array<ref LF_LedgerEntry> ledgerEntries = new array<ref LF_LedgerEntry>();
			if (context.Read(ledgerEntries))
			{
				LF_Ledger.ApplyAll(ledgerEntries);
				LF_Debug.Log("Persistence", string.Format("applied %1 ledger entr(ies)", ledgerEntries.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "money ledger failed to read, leaving the live ledger untouched");
			}
		}
		else if (payload == "crimerecord")
		{
			// A version 1-14 save never reaches this payload, so the live record is left alone.
			array<ref LF_CrimeRecordRow> crimeRows = new array<ref LF_CrimeRecordRow>();
			if (context.Read(crimeRows))
			{
				LF_CrimeRecord.ApplyAll(crimeRows);
				LF_Debug.Log("Persistence", string.Format("applied %1 crime record row(s)", crimeRows.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "crime record failed to read, leaving the live record untouched");
			}
		}
		else if (payload == "fenceserials")
		{
			// A version 1-15 save never reaches this payload, so the live book is left alone.
			array<ref LF_FenceSerialRow> serialRows = new array<ref LF_FenceSerialRow>();
			if (context.Read(serialRows))
			{
				LF_FenceSerials.ApplyAll(serialRows);
				LF_Debug.Log("Persistence", string.Format("applied %1 fence serial row(s)", serialRows.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "fence serial book failed to read, leaving the live book untouched");
			}
		}
		else if (payload == "incidentflags")
		{
			// A version 1-16 save never reaches this payload, so the live flags are left alone.
			array<ref LF_IncidentFlagRow> flagRows = new array<ref LF_IncidentFlagRow>();
			if (context.Read(flagRows))
			{
				LF_IncidentFlags.ApplyAll(flagRows);
				LF_Debug.Log("Persistence", string.Format("applied %1 incident flag(s)", flagRows.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "incident flags failed to read, leaving the live flags untouched");
			}
		}
		else if (payload == "evidencelocker")
		{
			// A version 1-17 save never reaches this payload, so the live case file is left alone.
			array<ref LF_EvidenceLockerRow> lockerRows = new array<ref LF_EvidenceLockerRow>();
			if (context.Read(lockerRows))
			{
				LF_EvidenceLocker.ApplyAll(lockerRows);
				LF_Debug.Log("Persistence", string.Format("applied %1 evidence locker row(s)", lockerRows.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "evidence locker failed to read, leaving the live case file untouched");
			}
		}
		else if (payload == "bills")
		{
			// A version 1-18 save never reaches this payload, so the live bills are left alone.
			array<ref LF_TreatmentBillRecord> billRecords = new array<ref LF_TreatmentBillRecord>();
			if (context.Read(billRecords))
			{
				LF_TreatmentBills.ApplyAll(billRecords);
				LF_Debug.Log("Persistence", string.Format("applied %1 treatment bill(s)", billRecords.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "treatment bills failed to read, leaving the live bills untouched");
			}
		}
		else if (payload == "chainstations")
		{
			// A version 1-19 save never reaches this payload, so the live stations are left alone.
			array<ref LF_ChainStationRow> stationRows = new array<ref LF_ChainStationRow>();
			if (context.Read(stationRows))
			{
				LF_ChainStations.ApplyAll(stationRows);
				LF_Debug.Log("Persistence", string.Format("applied %1 chain station batch(es)", stationRows.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "chain stations failed to read, leaving the live stations untouched");
			}
		}
		else if (payload == "storestock")
		{
			// A version 1-20 save never reaches this payload, so the live stock is left alone.
			array<ref LF_StoreStockRow> stockRows = new array<ref LF_StoreStockRow>();
			if (context.Read(stockRows))
			{
				LF_Stores.ApplyAll(stockRows);
				LF_Debug.Log("Persistence", string.Format("applied %1 store stock row(s)", stockRows.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "store stock failed to read, leaving the live stock untouched");
			}
		}
		else if (payload == "itemserials")
		{
			// A version 1-21 save never reaches this payload: restored items stay unstamped.
			array<ref LF_StorageItemSerialRecord> serialRecords = new array<ref LF_StorageItemSerialRecord>();
			if (context.Read(serialRecords))
			{
				int serialMatched = LF_StorageSnapshot.ApplyItemSerials(serialRecords);
				LF_Debug.Log("Persistence", string.Format("applied item serials to %1 of %2 snapshot(s)", serialMatched, serialRecords.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "item serials failed to read, restored items stay unstamped");
			}
		}
		else if (payload == "farmplots")
		{
			// A version 1-22 save never reaches this payload, so the live plots are left alone.
			array<ref LF_FarmPlotRow> plotRows = new array<ref LF_FarmPlotRow>();
			if (context.Read(plotRows))
			{
				LF_FarmPlots.ApplyAll(plotRows);
				LF_Debug.Log("Persistence", string.Format("applied %1 planted farm plot(s)", plotRows.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "farm plots failed to read, leaving the live plots untouched");
			}
		}
		else if (payload == "characters")
		{
			// A version 1-23 save never reaches this payload: the account prefix already gave each
			// account's one character the account uid as its key (slot 1).
			array<ref LF_CharacterSlotRecord> slotRows = new array<ref LF_CharacterSlotRecord>();
			if (context.Read(slotRows))
			{
				int applied = LF_PlayerAccountManager.ApplyCharacterSlots(slotRows);
				LF_Debug.Info("Persistence", string.Format("applied %1 of %2 character slot(s)", applied, slotRows.Count()));
			}
			else
			{
				LF_Debug.Error("Persistence", "character slots failed to read: characters keep the slot 1 default keys");
			}
		}
		else if (payload == "storeunpaid")
		{
			// A version 1-24 save never reaches this payload: the waiting tags are left alone and
			// goods carried over that restart are untagged.
			array<ref LF_StoreUnpaidRow> unpaidRows = new array<ref LF_StoreUnpaidRow>();
			if (context.Read(unpaidRows))
			{
				int boundNow = LF_Stores.ApplyUnpaid(unpaidRows);
				LF_Debug.Info("Persistence", string.Format("applied %1 unpaid store tag(s), %2 back on characters in the world now", unpaidRows.Count(), boundNow));
			}
			else
			{
				LF_Debug.Warn("Persistence", "unpaid store tags failed to read, leaving the waiting tags untouched");
			}
		}
		else if (payload == "playtime")
		{
			// A version 1-25 save never reaches this payload: the live counts are left alone.
			array<ref LF_PlaytimeRecord> playtimeRows = new array<ref LF_PlaytimeRecord>();
			if (context.Read(playtimeRows))
			{
				int playtimeApplied = LF_Playtime.ApplyAll(playtimeRows);
				LF_Debug.Info("Persistence", string.Format("applied %1 of %2 playtime row(s)", playtimeApplied, playtimeRows.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "playtime failed to read, leaving the live counts untouched");
			}
		}
		else if (payload == "orgranks")
		{
			// Runs after "organizations" in the same load. A version 1-26 save never reaches this
			// payload: its ranks keep the masks ApplyAll back-filled from the four bools.
			array<ref LF_OrganizationRankRow> rankRows = new array<ref LF_OrganizationRankRow>();
			if (context.Read(rankRows))
			{
				LF_OrganizationManager.ApplyRanks(rankRows);
			}
			else
			{
				LF_Debug.Warn("Persistence", "organization rank rows failed to read, ranks keep the rights from their records");
			}
		}
		else if (payload == "joblevels")
		{
			// Runs after "progression" in the same load, which seeded the store from the records'
			// pre-#749 fields; these rows replace that. A version 7-27 save never reaches this
			// payload and keeps the seed (its migration).
			array<ref LF_JobXpRow> jobXpRows = new array<ref LF_JobXpRow>();
			if (context.Read(jobXpRows))
			{
				int jobXpApplied = LF_JobLevels.ApplyAll(jobXpRows);
				LF_Debug.Info("Persistence", string.Format("applied %1 of %2 job xp row(s)", jobXpApplied, jobXpRows.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "job xp rows failed to read, job levels keep the values the progression records hold");
			}
		}
		else if (payload == "wantedlooks")
		{
			// Runs after "wanted" in the same load, so the reports exist; matched by sequence. A
			// version 1-28 save never reaches this payload and its posters have no picture.
			array<ref LF_WantedLookRow> lookRows = new array<ref LF_WantedLookRow>();
			if (context.Read(lookRows))
			{
				int looksApplied = LF_WantedReports.ApplyLooks(lookRows);
				LF_Debug.Info("Persistence", string.Format("applied %1 of %2 wanted look(s)", looksApplied, lookRows.Count()));
			}
			else
			{
				LF_Debug.Warn("Persistence", "wanted looks failed to read, the posters show no picture until the next crime");
			}
		}
		else
		{
			LF_Debug.Error("Persistence", string.Format("load skipped unknown payload '%1'", payload));
		}
	}
}
