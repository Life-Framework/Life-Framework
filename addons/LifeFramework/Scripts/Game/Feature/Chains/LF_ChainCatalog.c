//------------------------------------------------------------------------------------------------
//! Loads the chains in ResourceChains.conf, validates each one, and answers lookups. A chain
//! that fails validation logs one LF_Debug.Error and is disabled; every other chain still runs.
//! Validation is split: Validate is pure (LOGIC-testable), ResourcesResolve loads prefabs.
class LF_ChainCatalog
{
	static const string FEATURE = "Chains";
	static const int MAX_TIER = 5;

	protected static ref map<string, ref LF_ChainDef> s_mEnabled;
	protected static ref array<string> s_aDisabled;
	protected static bool s_bLoaded;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mEnabled = null;
		s_aDisabled = null;
		s_bLoaded = false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Empty when the chain row is well formed, else the first problem found. Pure: no
	//! resource loading and no logging.
	static string Validate(LF_ChainDef chain)
	{
		if (!chain)
			return "null chain row";

		if (chain.m_sChainId.IsEmpty())
			return "empty chain id";

		if (chain.m_iTier < 1 || chain.m_iTier > MAX_TIER)
			return string.Format("tier %1 is outside 1..%2", chain.m_iTier, MAX_TIER);

		if (!chain.m_aSteps || chain.m_aSteps.IsEmpty())
			return "no steps";

		array<string> stepIds = {};
		foreach (LF_ChainStepDef step : chain.m_aSteps)
		{
			string stepError = ValidateStep(chain, step);
			if (!stepError.IsEmpty())
				return stepError;

			if (stepIds.Contains(step.m_sStepId))
				return string.Format("duplicate step id %1", step.m_sStepId);

			stepIds.Insert(step.m_sStepId);
		}

		if (chain.m_aEquipment)
		{
			array<string> equipmentIds = {};
			foreach (LF_ChainEquipmentDef equipment : chain.m_aEquipment)
			{
				if (!equipment || equipment.m_sEquipmentId.IsEmpty() || equipment.m_Prefab.IsEmpty())
					return "equipment row with no id or prefab";

				if (equipment.m_iPrice < 0)
					return string.Format("equipment %1 has a negative price", equipment.m_sEquipmentId);

				if (equipmentIds.Contains(equipment.m_sEquipmentId))
					return string.Format("duplicate equipment id %1", equipment.m_sEquipmentId);

				equipmentIds.Insert(equipment.m_sEquipmentId);
			}
		}

		LF_ChainHeatDef heat = chain.m_Heat;
		if (heat)
		{
			if (heat.m_fPerWork < 0 || heat.m_fDecayPerSecond < 0 || heat.m_fMax < 0 || heat.m_fRangeM < 0)
				return "negative heat setting";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected static string ValidateStep(LF_ChainDef chain, LF_ChainStepDef step)
	{
		if (!step)
			return "null step row";

		if (step.m_sStepId.IsEmpty())
			return "step with no id";

		if (step.m_sStationType.IsEmpty())
			return string.Format("step %1 names no station type", step.m_sStepId);

		if (step.m_iWork < 1)
			return string.Format("step %1 needs less than 1 work unit", step.m_sStepId);

		if (step.m_iMinLevel < 0)
			return string.Format("step %1 has a negative level gate", step.m_sStepId);

		if (!LF_ProcessAction.IsRecipeConfigured(step.m_aInputs, step.m_aOutputs))
			return string.Format("step %1 has an empty or malformed recipe", step.m_sStepId);

		if (!step.m_sEquipmentId.IsEmpty() && !chain.FindEquipment(step.m_sEquipmentId))
			return string.Format("step %1 needs equipment %2 the chain does not list", step.m_sStepId, step.m_sEquipmentId);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Every prefab a chain names must load. Run at catalog load, not per press.
	static bool ResourcesResolve(LF_ChainDef chain)
	{
		foreach (LF_ChainStepDef step : chain.m_aSteps)
		{
			if (!LF_ProcessAction.AreRecipeResourcesAvailable(step.m_aInputs, step.m_aOutputs))
				return false;
		}

		if (chain.m_aEquipment)
		{
			foreach (LF_ChainEquipmentDef equipment : chain.m_aEquipment)
			{
				Resource resource = Resource.Load(equipment.m_Prefab);
				if (!resource || !resource.IsValid())
					return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds the enabled set from a config. Tests pass their own config; the game passes the one
	//! in ResourceChains.conf. \param checkResources False skips prefab loading (LOGIC tests).
	static void Build(LF_ResourceChainConfig config, bool checkResources = true)
	{
		s_mEnabled = new map<string, ref LF_ChainDef>();
		s_aDisabled = {};
		s_bLoaded = true;

		if (!config || !config.m_aChains)
			return;

		foreach (LF_ChainDef chain : config.m_aChains)
		{
			string error = Validate(chain);
			if (error.IsEmpty() && s_mEnabled.Contains(chain.m_sChainId))
				error = string.Format("duplicate chain id %1", chain.m_sChainId);

			if (error.IsEmpty() && checkResources && !ResourcesResolve(chain))
				error = "a prefab does not resolve";

			if (!error.IsEmpty())
			{
				string label = "?";
				if (chain && !chain.m_sChainId.IsEmpty())
					label = chain.m_sChainId;

				LF_Debug.Error(FEATURE, string.Format("chain %1 disabled: %2", label, error));
				if (label != "?" && !s_mEnabled.Contains(label) && !s_aDisabled.Contains(label))
					s_aDisabled.Insert(label);
				continue;
			}

			s_mEnabled.Set(chain.m_sChainId, chain);
			LF_Debug.Info(FEATURE, string.Format("chain %1 enabled (tier %2, %3 steps)", chain.m_sChainId, chain.m_iTier, chain.m_aSteps.Count()));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void EnsureLoaded()
	{
		if (!s_bLoaded)
			Build(LF_ResourceChainCatalog.GetConfig());
	}

	//------------------------------------------------------------------------------------------------
	//! \return The enabled chain, or null for an unknown or disabled id.
	static LF_ChainDef FindChain(string chainId)
	{
		EnsureLoaded();
		if (chainId.IsEmpty())
			return null;

		return s_mEnabled.Get(chainId);
	}

	//------------------------------------------------------------------------------------------------
	static bool IsEnabled(string chainId)
	{
		return FindChain(chainId) != null;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsDisabled(string chainId)
	{
		EnsureLoaded();
		return s_aDisabled.Contains(chainId);
	}

	//------------------------------------------------------------------------------------------------
	//! Fills outChains with every enabled chain.
	static void GetEnabled(notnull array<LF_ChainDef> outChains)
	{
		EnsureLoaded();
		outChains.Clear();
		foreach (string id, LF_ChainDef chain : s_mEnabled)
		{
			outChains.Insert(chain);
		}
	}

	//------------------------------------------------------------------------------------------------
	static int EnabledCount()
	{
		EnsureLoaded();
		return s_mEnabled.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The step of an enabled chain, or null.
	static LF_ChainStepDef FindStep(string chainId, string stepId)
	{
		LF_ChainDef chain = FindChain(chainId);
		if (!chain)
			return null;

		return chain.FindStep(stepId);
	}
}
