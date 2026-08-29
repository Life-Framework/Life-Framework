//------------------------------------------------------------------------------------------------
//! Persists the game-mode persistence store (EL_PersistenceComponent): player accounts, bank
//! accounts and survival stats.
//!
//! BINDING. Listed in the ComponentSerializers block of Configs/Systems/Persistence/LifeFramework.conf,
//! matching the game-mode entity that carries EL_PersistenceComponent.
//!
//! WHERE THE DATA LIVES. Account and bank records are pulled from and applied to the manager
//! singletons directly - EL_PlayerAccountManager / EL_ATMManager own those caches, not this
//! component. Only survival stats are read from and written into the component's own map
//! (keyed by character persistence id).
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

		context.WriteValue("version", 1);

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
			return true;

		array<ref EL_PlayerAccountRecord> accountRecords = new array<ref EL_PlayerAccountRecord>();
		context.Read(accountRecords);
		if (accountRecords)
			EL_PlayerAccountManager.ApplyAll(accountRecords);

		array<ref EL_BankAccountRecord> bankRecords = new array<ref EL_BankAccountRecord>();
		context.Read(bankRecords);
		if (bankRecords)
			EL_ATMManager.ApplyAll(bankRecords);

		// Survival stats are owned by the component. Clear before rebuilding so re-applying to a
		// live session yields exactly the saved map (idempotent).
		array<ref EL_SurvivalStatsRecord> survivalRecords = new array<ref EL_SurvivalStatsRecord>();
		context.Read(survivalRecords);
		if (survivalRecords)
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
		}

		return true;
	}
}