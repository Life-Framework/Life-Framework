//------------------------------------------------------------------------------------------------
//! Persists the game-mode persistence store (EL_PersistenceComponent): player accounts, bank
//! accounts, survival stats, organizations, and owned vehicle records.
//!
//! BINDING. Listed in the ComponentSerializers block of Configs/Systems/Persistence/LifeFramework.conf,
//! matching the game-mode entity that carries EL_PersistenceComponent.
//!
//! WHERE THE DATA LIVES. Account, bank, and organization records are pulled from and applied to
//! the manager singletons directly - EL_PlayerAccountManager / EL_ATMManager /
//! EL_OrganizationManager own those caches, not this component. Only survival stats are read
//! from and written into the component's own map (keyed by character persistence id).
//! Vehicle records live in EL_VehicleOwnership, keyed by the lock identifier.
//! Version 2 appends organizations. Version 3 appends vehicles after that array.
//! A version 1 save has neither payload. A version 2 save has organizations and no vehicles.
//!
//! FORMAT. Binary save contexts are POSITIONAL: write order must equal read order. Version first,
//! hand-rolled, as in every vanilla serializer. A missing version means this component has no
//! payload in the stored record (a save written before this serializer existed), and every
//! serializer bails there rather than applying zero-valued defaults.
//!
//! IDEMPOTENT ON A LIVE SESSION. Deserialize also runs when saved data is re-applied to a running
//! session. The manager ApplyAll() implementations are plain replacement of cached state, and the
//! survival map is cleared before being rebuilt, so a second pass produces exactly the same end
//! state as the first.
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

		context.WriteValue("version", 3);

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

		// Survival stats live on the persistence component itself.
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

		// Version 2 appends organizations. A version 1 save has nothing after survival.
		array<ref EL_OrganizationRecord> orgRecords = EL_OrganizationManager.ExportAll();
		if (!orgRecords)
			orgRecords = new array<ref EL_OrganizationRecord>();
		context.Write(orgRecords);

		// Version 3 appends vehicles after organizations.
		array<ref EL_VehicleRecord> vehicleRecords = EL_VehicleOwnership.ExportAll();
		if (!vehicleRecords)
			vehicleRecords = new array<ref EL_VehicleRecord>();
		context.Write(vehicleRecords);

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

		// Version 1 saves end at survival. Leave live organizations alone so an older
		// payload cannot wipe a fund that the save never knew about.
		if (version >= 2)
		{
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

		// Version 2 saves end at organizations. Leave live vehicles alone so that
		// payload cannot wipe a vehicle the save never knew about.
		if (version >= 3)
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

		return true;
	}
}