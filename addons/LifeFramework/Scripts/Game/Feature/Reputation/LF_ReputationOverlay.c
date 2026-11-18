//------------------------------------------------------------------------------------------------
//! Pack overlays for the standing tables (#3044). `LF_ReputationConfig.Get()` and
//! `LF_ReputationContractsConfig.Get()` call Apply / ApplyContracts after loading the core file;
//! each pack overlay the mission header lists (`m_aLF_ConfigOverlays`, load order) is layered over
//! it through the shared merge (LF_ConfigOverlay, docs/config/override-model.md). A pack retunes a
//! faction's words, awards, gates, pay and contracts by id: add, replace, patch (only the fields the
//! entry sets) or remove. Pack overlays only: there is no owner config.json section.
//!
//! The lists and their record keys: factions `m_sId`, awards `m_sAction`, gates `m_sGate`,
//! pays `m_sFaction`, contracts `m_sId`. The contracts file's `m_fCooldownSeconds` stays core.
class LF_ReputationOverlay
{
	static const string FEATURE = "Reputation";
	static const string SECTION = "reputation";
	static const string SECTION_CONTRACTS = "reputationContracts";

	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay of LF_ReputationConfig over `config`, four lists per overlay.
	static void Apply(notnull LF_ReputationConfig config)
	{
		if (!config.m_aFactions)
			config.m_aFactions = new array<ref LF_ReputationFactionDef>();

		if (!config.m_aAwards)
			config.m_aAwards = new array<ref LF_ReputationAwardDef>();

		if (!config.m_aGates)
			config.m_aGates = new array<ref LF_ReputationGateDef>();

		if (!config.m_aPays)
			config.m_aPays = new array<ref LF_ReputationPayDef>();

		LF_ReputationFactionList factions = new LF_ReputationFactionList(config);
		LF_ReputationAwardList awards = new LF_ReputationAwardList(config);
		LF_ReputationGateList gates = new LF_ReputationGateList(config);
		LF_ReputationPayList pays = new LF_ReputationPayList(config);
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(LF_ReputationConfig.CONFIG_PATH, LF_ReputationConfig, SECTION, resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			BaseContainer root = resource.GetResource().ToBaseContainer();
			int changed = LF_ConfigOverlay.ApplyContainerList(factions, root, "m_aFactions", names[i], claims);
			changed += LF_ConfigOverlay.ApplyContainerList(awards, root, "m_aAwards", names[i], claims);
			changed += LF_ConfigOverlay.ApplyContainerList(gates, root, "m_aGates", names[i], claims);
			changed += LF_ConfigOverlay.ApplyContainerList(pays, root, "m_aPays", names[i], claims);
			LF_Debug.Info(FEATURE, string.Format("pack overlay %1 changed %2 standing rows", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay of LF_ReputationContractsConfig over `config` (its contract list).
	static void ApplyContracts(notnull LF_ReputationContractsConfig config)
	{
		if (!config.m_aContracts)
			config.m_aContracts = new array<ref LF_ReputationContractDef>();

		LF_ReputationContractList contracts = new LF_ReputationContractList(config);
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(LF_ReputationContractsConfig.CONFIG_PATH, LF_ReputationContractsConfig, SECTION_CONTRACTS, resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = LF_ConfigOverlay.ApplyContainerList(contracts, resource.GetResource().ToBaseContainer(), "m_aContracts", names[i], claims);
			LF_Debug.Info(FEATURE, string.Format("pack overlay %1 changed %2 contract rows", names[i], changed));
		}
	}
}

//------------------------------------------------------------------------------------------------
//! `m_aFactions` for the shared merge, keyed by faction id (TOWN, POLICE, UNDERWORLD).
class LF_ReputationFactionList : LF_ConfigList
{
	protected LF_ReputationConfig m_Config;

	void LF_ReputationFactionList(LF_ReputationConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "reputation.factions";
	}

	override int Count()
	{
		return m_Config.m_aFactions.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aFactions[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_ReputationFactionDef.Cast(record).m_sId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sId", id);
		return id;
	}

	override string IdHint()
	{
		return "a faction id such as TOWN";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		LF_ReputationFactionDef faction = new LF_ReputationFactionDef();
		faction.m_sId = id;
		return faction;
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_ReputationFactionDef faction = LF_ReputationFactionDef.Cast(record);
		if (index < m_Config.m_aFactions.Count())
			m_Config.m_aFactions.Set(index, faction);
		else
			m_Config.m_aFactions.Insert(faction);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aFactions.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_ReputationFactionDef faction = LF_ReputationFactionDef.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_sLabel") && container.Get("m_sLabel", faction.m_sLabel))
			changed++;

		if (container.IsVariableSetDirectly("m_aTierThresholds"))
		{
			array<int> thresholds = {};
			if (container.Get("m_aTierThresholds", thresholds))
			{
				faction.m_aTierThresholds = {};
				faction.m_aTierThresholds.Copy(thresholds);
				changed++;
			}
		}

		if (container.IsVariableSetDirectly("m_aTierLabels"))
		{
			array<string> labels = {};
			if (container.Get("m_aTierLabels", labels))
			{
				faction.m_aTierLabels = {};
				faction.m_aTierLabels.Copy(labels);
				changed++;
			}
		}

		if (container.IsVariableSetDirectly("m_bShown") && container.Get("m_bShown", faction.m_bShown))
			changed++;

		if (container.IsVariableSetDirectly("m_iDailyCap") && container.Get("m_iDailyCap", faction.m_iDailyCap))
			changed++;

		return changed;
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		LF_ReputationFactionDef faction = LF_ReputationFactionDef.Cast(record);
		if (!faction.m_aTierThresholds || faction.m_aTierThresholds.IsEmpty())
			return "a faction the shipped config does not list needs m_aTierThresholds";

		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! `m_aAwards` for the shared merge, keyed by action id (job_completed, fence_sale).
class LF_ReputationAwardList : LF_ConfigList
{
	protected LF_ReputationConfig m_Config;

	void LF_ReputationAwardList(LF_ReputationConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "reputation.awards";
	}

	override int Count()
	{
		return m_Config.m_aAwards.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aAwards[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_ReputationAwardDef.Cast(record).m_sAction;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sAction", id);
		return id;
	}

	override string IdHint()
	{
		return "an action id such as job_completed:PARCEL";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		LF_ReputationAwardDef award = new LF_ReputationAwardDef();
		award.m_sAction = id;
		return award;
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_ReputationAwardDef award = LF_ReputationAwardDef.Cast(record);
		if (index < m_Config.m_aAwards.Count())
			m_Config.m_aAwards.Set(index, award);
		else
			m_Config.m_aAwards.Insert(award);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aAwards.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_ReputationAwardDef award = LF_ReputationAwardDef.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_sFaction") && container.Get("m_sFaction", award.m_sFaction))
			changed++;

		if (container.IsVariableSetDirectly("m_iPoints") && container.Get("m_iPoints", award.m_iPoints))
			changed++;

		if (container.IsVariableSetDirectly("m_fCooldownSeconds") && container.Get("m_fCooldownSeconds", award.m_fCooldownSeconds))
			changed++;

		return changed;
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		if (LF_ReputationAwardDef.Cast(record).m_sFaction.IsEmpty())
			return "an award needs a m_sFaction";

		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! `m_aGates` for the shared merge, keyed by gate id (job:TAXI, contract:<id>).
class LF_ReputationGateList : LF_ConfigList
{
	protected LF_ReputationConfig m_Config;

	void LF_ReputationGateList(LF_ReputationConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "reputation.gates";
	}

	override int Count()
	{
		return m_Config.m_aGates.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aGates[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_ReputationGateDef.Cast(record).m_sGate;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sGate", id);
		return id;
	}

	override string IdHint()
	{
		return "a gate id such as job:TAXI or contract:<id>";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		LF_ReputationGateDef gate = new LF_ReputationGateDef();
		gate.m_sGate = id;
		return gate;
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_ReputationGateDef gate = LF_ReputationGateDef.Cast(record);
		if (index < m_Config.m_aGates.Count())
			m_Config.m_aGates.Set(index, gate);
		else
			m_Config.m_aGates.Insert(gate);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aGates.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_ReputationGateDef gate = LF_ReputationGateDef.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_sFaction") && container.Get("m_sFaction", gate.m_sFaction))
			changed++;

		if (container.IsVariableSetDirectly("m_iMinTier") && container.Get("m_iMinTier", gate.m_iMinTier))
			changed++;

		return changed;
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		if (LF_ReputationGateDef.Cast(record).m_sFaction.IsEmpty())
			return "a gate needs a m_sFaction";

		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! `m_aPays` for the shared merge, keyed by faction id.
class LF_ReputationPayList : LF_ConfigList
{
	protected LF_ReputationConfig m_Config;

	void LF_ReputationPayList(LF_ReputationConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "reputation.pays";
	}

	override int Count()
	{
		return m_Config.m_aPays.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aPays[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_ReputationPayDef.Cast(record).m_sFaction;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sFaction", id);
		return id;
	}

	override string IdHint()
	{
		return "a faction id such as TOWN";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		LF_ReputationPayDef pay = new LF_ReputationPayDef();
		pay.m_sFaction = id;
		return pay;
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_ReputationPayDef pay = LF_ReputationPayDef.Cast(record);
		if (index < m_Config.m_aPays.Count())
			m_Config.m_aPays.Set(index, pay);
		else
			m_Config.m_aPays.Insert(pay);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aPays.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_ReputationPayDef pay = LF_ReputationPayDef.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_aPayPercentByTier"))
		{
			array<int> percents = {};
			if (container.Get("m_aPayPercentByTier", percents))
			{
				pay.m_aPayPercentByTier = {};
				pay.m_aPayPercentByTier.Copy(percents);
				changed++;
			}
		}

		return changed;
	}
}

//------------------------------------------------------------------------------------------------
//! `m_aContracts` for the shared merge, keyed by contract id.
class LF_ReputationContractList : LF_ConfigList
{
	protected LF_ReputationContractsConfig m_Config;

	void LF_ReputationContractList(LF_ReputationContractsConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "reputationContracts.contracts";
	}

	override int Count()
	{
		return m_Config.m_aContracts.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aContracts[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_ReputationContractDef.Cast(record).m_sId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sId", id);
		return id;
	}

	override string IdHint()
	{
		return "a contract id";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		LF_ReputationContractDef contract = new LF_ReputationContractDef();
		contract.m_sId = id;
		return contract;
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_ReputationContractDef contract = LF_ReputationContractDef.Cast(record);
		if (index < m_Config.m_aContracts.Count())
			m_Config.m_aContracts.Set(index, contract);
		else
			m_Config.m_aContracts.Insert(contract);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aContracts.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_ReputationContractDef contract = LF_ReputationContractDef.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_sJob") && container.Get("m_sJob", contract.m_sJob))
			changed++;

		if (container.IsVariableSetDirectly("m_sStandingGate") && container.Get("m_sStandingGate", contract.m_sStandingGate))
			changed++;

		if (container.IsVariableSetDirectly("m_sFaction") && container.Get("m_sFaction", contract.m_sFaction))
			changed++;

		if (container.IsVariableSetDirectly("m_fPayMultiplier") && container.Get("m_fPayMultiplier", contract.m_fPayMultiplier))
			changed++;

		if (container.IsVariableSetDirectly("m_iAwardPoints") && container.Get("m_iAwardPoints", contract.m_iAwardPoints))
			changed++;

		if (container.IsVariableSetDirectly("m_iStakePoints") && container.Get("m_iStakePoints", contract.m_iStakePoints))
			changed++;

		if (container.IsVariableSetDirectly("m_fTimeLimitSeconds") && container.Get("m_fTimeLimitSeconds", contract.m_fTimeLimitSeconds))
			changed++;

		return changed;
	}
}
