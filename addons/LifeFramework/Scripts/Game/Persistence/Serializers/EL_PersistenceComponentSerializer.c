//------------------------------------------------------------------------------------------------
//! Persists the game-mode persistence store (EL_PersistenceComponent): player accounts, bank
//! accounts, survival stats, organizations, hideout ownership, and owned vehicle records.
//!
//! BINDING. Listed in the ComponentSerializers block of Configs/Systems/Persistence/LifeFramework.conf,
//! matching the game-mode entity that carries EL_PersistenceComponent.
//!
//! WHERE THE DATA LIVES. Account, bank, and organization records are pulled from and applied to
//! the manager singletons directly - EL_PlayerAccountManager / EL_ATMManager /
//! EL_OrganizationManager own those caches, not this component. Only survival stats are read
//! from and written into the component's own map (keyed by character persistence id).
//! Hideout records live in LF_Hideout. Vehicle records live in EL_VehicleOwnership, keyed by
//! the lock identifier.
//!
//! FORMAT. Binary save contexts are POSITIONAL: write order must equal read order. Version is
//! written first. Accounts and bank are the prefix on every save, including version 1.
//! RegisterPersistencePayloads is the only order after that prefix: survival, then organizations,
//! hideout, vehicles. The written version is that list's length (4). Serialize walks the whole
//! list. Deserialize walks the same list and stops at the version stored in the save.
//! A version 1 save stops after survival (no organization payload). A version 2 save includes
//! organizations and has no hideouts. A version 3 save includes hideouts and has no vehicles.
//! The next system adds one Insert at the end of the list and raises the written version by one.
//! Do not insert earlier and do not reorder: existing saves are positional.
//!
//! IDEMPOTENT ON A LIVE SESSION. Deserialize also runs when saved data is re-applied to a running
//! session. The manager ApplyAll() implementations are plain replacement of cached state, and the
//! survival map is cleared before being rebuilt, so a second pass produces exactly the same end
//! state as the first. A payload the stored version does not contain is left untouched.
//------------------------------------------------------------------------------------------------
class EL_PersistenceComponentSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return EL_PersistenceComponent;
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
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the account, bank and survival records into the save context.
	//! \param[in] owner The game-mode entity owning the persistence component.
	//! \param[in] component The persistence component being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an EL_PersistenceComponent.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		EL_PersistenceComponent persistence = EL_PersistenceComponent.Cast(component);
		if (!persistence)
			return ESerializeResult.ERROR;

		array<string> order = new array<string>();
		RegisterPersistencePayloads(order);
		context.WriteValue("version", 4);

		// Accounts live in the player account manager singleton. A null result (no manager / no
		// data) is written as an empty array so the positional read order stays intact.
		array<ref EL_PlayerAccountRecord> accountRecords = EL_PlayerAccountManager.ExportAll();
		if (!accountRecords)
			accountRecords = new array<ref EL_PlayerAccountRecord>();
		context.Write(accountRecords);

		// Bank accounts live in the ATM manager singleton.
		array<ref EL_BankAccountRecord> bankRecords = EL_ATMManager.ExportAll();
		if (!bankRecords)
			bankRecords = new array<ref EL_BankAccountRecord>();
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
		EL_PersistenceComponent persistence = EL_PersistenceComponent.Cast(component);
		if (!persistence)
			return false;

		array<string> order = new array<string>();
		RegisterPersistencePayloads(order);

		// No version means this component has no payload in the stored record - leave the live
		// session alone.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
		{
			EL_Debug.Warn("Persistence", "load skipped: no version header (save predates the serializer)");
			return true;
		}

		array<ref EL_PlayerAccountRecord> accountRecords = new array<ref EL_PlayerAccountRecord>();
		if (context.Read(accountRecords))
		{
			EL_PlayerAccountManager.ApplyAll(accountRecords);
			EL_Debug.Log("Persistence", string.Format("applied %1 account record(s)", accountRecords.Count()));
		}
		else
		{
			EL_Debug.Warn("Persistence", "account records failed to read, leaving live accounts untouched");
		}

		array<ref EL_BankAccountRecord> bankRecords = new array<ref EL_BankAccountRecord>();
		if (context.Read(bankRecords))
		{
			EL_ATMManager.ApplyAll(bankRecords);
			EL_Debug.Log("Persistence", string.Format("applied %1 bank account record(s)", bankRecords.Count()));
		}
		else
		{
			EL_Debug.Warn("Persistence", "bank records failed to read, leaving live balances untouched");
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
	protected static void WritePersistencePayload(notnull EL_PersistenceComponent persistence, notnull SaveContext context, string payload)
	{
		if (payload == "survival")
		{
			array<ref EL_SurvivalStatsRecord> survivalRecords = new array<ref EL_SurvivalStatsRecord>();
			map<string, ref EL_SurvivalStats> survivalMap = persistence.GetSurvivalStatsMap();
			if (survivalMap)
			{
				foreach (string characterId, EL_SurvivalStats stats : survivalMap)
				{
					if (!stats)
						continue;

					EL_SurvivalStatsRecord record = EL_SurvivalStatsRecord.Create(
						characterId,
						stats.GetHunger(),
						stats.GetThirst(),
						stats.GetHealth()
					);
					survivalRecords.Insert(record);
				}
			}
			context.Write(survivalRecords);
		}
		else if (payload == "organizations")
		{
			array<ref EL_OrganizationRecord> orgRecords = EL_OrganizationManager.ExportAll();
			if (!orgRecords)
				orgRecords = new array<ref EL_OrganizationRecord>();
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
			array<ref EL_VehicleRecord> vehicleRecords = EL_VehicleOwnership.ExportAll();
			if (!vehicleRecords)
				vehicleRecords = new array<ref EL_VehicleRecord>();
			context.Write(vehicleRecords);
		}
		else
		{
			EL_Debug.Error("Persistence", string.Format("save skipped unknown payload '%1'", payload));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Reads one registered payload and stops the caller from applying anything past the stored version.
	//! \param[in] persistence Persistence component that owns survival stats.
	//! \param[in] context Load context to read from.
	//! \param[in] payload Name from RegisterPersistencePayloads.
	protected static void ReadPersistencePayload(notnull EL_PersistenceComponent persistence, notnull LoadContext context, string payload)
	{
		if (payload == "survival")
		{
			// Survival stats are owned by the component. Clear before rebuilding so re-applying to a
			// live session yields exactly the saved map (idempotent). Only clear when the read
			// succeeded, so a corrupt payload does not wipe live stats.
			array<ref EL_SurvivalStatsRecord> survivalRecords = new array<ref EL_SurvivalStatsRecord>();
			if (context.Read(survivalRecords))
			{
				persistence.ClearSurvivalStats();
				foreach (EL_SurvivalStatsRecord record : survivalRecords)
				{
					if (!record)
						continue;

					EL_SurvivalStats stats = EL_SurvivalStats.Create(record.m_sPersistentId);
					stats.SetHunger(record.m_fHunger);
					stats.SetThirst(record.m_fThirst);
					stats.SetHealth(record.m_fHealth);
					persistence.SetSurvivalStats(record.m_sPersistentId, stats);
				}
				EL_Debug.Log("Persistence", string.Format("applied %1 survival record(s)", survivalRecords.Count()));
			}
			else
			{
				EL_Debug.Warn("Persistence", "survival records failed to read, leaving live stats untouched");
			}
		}
		else if (payload == "organizations")
		{
			// A version 1 save never reaches this payload, so a live fund the save never
			// knew about is left alone.
			array<ref EL_OrganizationRecord> orgRecords = new array<ref EL_OrganizationRecord>();
			if (context.Read(orgRecords))
			{
				EL_OrganizationManager.ApplyAll(orgRecords);
				EL_Debug.Log("Persistence", string.Format("applied %1 organization record(s)", orgRecords.Count()));
			}
			else
			{
				EL_Debug.Warn("Persistence", "organization records failed to read, leaving live organizations untouched");
			}
		}
		else if (payload == "hideout")
		{
			array<ref LF_HideoutRecord> hideoutRecords = new array<ref LF_HideoutRecord>();
			if (context.Read(hideoutRecords))
			{
				LF_Hideout.ApplyAll(hideoutRecords);
				EL_Debug.Log("Persistence", string.Format("applied %1 hideout record(s)", hideoutRecords.Count()));
			}
			else
			{
				EL_Debug.Warn("Persistence", "hideout records failed to read, leaving live hideouts untouched");
			}
		}
		else if (payload == "vehicles")
		{
			array<ref EL_VehicleRecord> vehicleRecords = new array<ref EL_VehicleRecord>();
			if (context.Read(vehicleRecords))
			{
				EL_VehicleOwnership.ApplyAll(vehicleRecords);
				EL_VehicleOwnership.RestoreWorld();
				EL_Debug.Log("Persistence", string.Format("applied %1 vehicle record(s)", vehicleRecords.Count()));
			}
			else
			{
				EL_Debug.Warn("Persistence", "vehicle records failed to read, leaving live vehicles untouched");
			}
		}
		else
		{
			EL_Debug.Error("Persistence", string.Format("load skipped unknown payload '%1'", payload));
		}
	}
}
