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
//! hideout, vehicles, jail, carried, progression, introductions, itemstate, wanted, jobsignups, tickets, incidents, ledger, crimerecord, fenceserials, incidentflags. The written version is that list's length (17). Serialize walks the
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

		return SerializeStore(persistence, context);
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
		context.WriteValue("version", 17);

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

		foreach (string payload : order)
		{
			WritePersistencePayload(persistence, context, payload);
		}

		return ESerializeResult.OK;
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

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes one registered payload. The list, not this function, decides the order.
	//! \param[in] persistence Persistence component that owns survival stats.
	//! \param[in] context Save context to write into.
	//! \param[in] payload Name from RegisterPersistencePayloads.
	static void WritePersistencePayload(notnull LF_PersistenceComponent persistence, notnull SaveContext context, string payload)
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
		else
		{
			LF_Debug.Error("Persistence", string.Format("load skipped unknown payload '%1'", payload));
		}
	}
}
