//------------------------------------------------------------------------------------------------
//! Why a worker may not start a step. The server evaluates this before any work counts.
enum LF_EChainGate
{
	OK,
	LICENSE,
	LEVEL,
	EQUIPMENT
}

//------------------------------------------------------------------------------------------------
//! The licence, level and equipment gates of a step. Evaluate is pure so a LOGIC test can drive
//! it; ForPlayer reads the real player on the server.
class LF_ChainGate
{
	//------------------------------------------------------------------------------------------------
	//! \param hasLicense Whether the worker holds step.m_eLicense. Ignored when the step names none.
	static LF_EChainGate Evaluate(notnull LF_ChainStepDef step, bool hasLicense, int level, bool hasEquipment)
	{
		if (step.m_eLicense != LF_ELicenseType.UNEMPLOYED && !hasLicense)
			return LF_EChainGate.LICENSE;

		if (step.m_iMinLevel > 0 && level < step.m_iMinLevel)
			return LF_EChainGate.LEVEL;

		if (!step.m_sEquipmentId.IsEmpty() && !hasEquipment)
			return LF_EChainGate.EQUIPMENT;

		return LF_EChainGate.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: reads the worker's licences, level and inventory. The caller logs a refusal with
	//! the worker's uid; this only decides.
	static LF_EChainGate ForPlayer(IEntity player, notnull LF_ChainDef chain, notnull LF_ChainStepDef step)
	{
		if (!player)
			return LF_EChainGate.LICENSE;

		bool hasLicense = true;
		if (step.m_eLicense != LF_ELicenseType.UNEMPLOYED)
		{
			LF_LicenseManagerComponent licenses = LF_Component<LF_LicenseManagerComponent>.Find(player);
			hasLicense = licenses && licenses.HasLicense(step.m_eLicense);
		}

		int level = 0;
		if (step.m_iMinLevel > 0)
			level = LF_JobLevels.LevelOfBody(player, LevelJob(step));

		bool hasEquipment = true;
		if (!step.m_sEquipmentId.IsEmpty())
		{
			hasEquipment = false;
			LF_ChainEquipmentDef equipment = chain.FindEquipment(step.m_sEquipmentId);
			if (equipment)
			{
				InventoryStorageManagerComponent inventory = LF_Component<InventoryStorageManagerComponent>.Find(player);
				hasEquipment = inventory && LF_InventoryUtils.GetAmount(inventory, equipment.m_Prefab) > 0;
			}
		}

		return Evaluate(step, hasLicense, level, hasEquipment);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The job a step's level gate reads; CIVILIAN when the step names none.
	static string LevelJob(notnull LF_ChainStepDef step)
	{
		if (step.m_sLevelJob.IsEmpty())
			return LF_JobLevels.JOB_CIVILIAN;

		return step.m_sLevelJob;
	}

	//------------------------------------------------------------------------------------------------
	//! The words for a level refusal: the step's job and how far short the worker is (#2700),
	//! never the level itself.
	static string LevelKey(notnull LF_ChainStepDef step, int level)
	{
		return LF_JobLevelWords.Refusal(LevelJob(step), level, step.m_iMinLevel);
	}

	//------------------------------------------------------------------------------------------------
	//! The line a licence refusal appends (#3150): where to get the step's licence and what it costs,
	//! or which job unlocks it. Empty when the step names no licence.
	static string LicenceWhere(notnull LF_ChainStepDef step)
	{
		if (step.m_eLicense == LF_ELicenseType.UNEMPLOYED)
			return string.Empty;

		return LF_LicenceWhere.Line(step.m_eLicense);
	}

	//------------------------------------------------------------------------------------------------
	//! Localization key shown to the worker for a refusal. Empty for OK. \param step and
	//! \param level (the worker's level in the step's job) pick the words of a level refusal.
	static string ReasonKey(LF_EChainGate gate, LF_ChainStepDef step = null, int level = 0)
	{
		switch (gate)
		{
			case LF_EChainGate.LICENSE: return "#LF-Chain_NeedLicense";
			case LF_EChainGate.LEVEL:
			{
				if (!step)
					return "#LF-Chain_NeedLevel";

				return LevelKey(step, level);
			}
			case LF_EChainGate.EQUIPMENT: return "#LF-Chain_NeedEquipment";
		}

		return "";
	}
}
