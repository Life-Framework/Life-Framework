//------------------------------------------------------------------------------------------------
//! Job XP for a finished machine batch (#4337, 0.3.0 W2, decision N3). A machine row names the
//! `jobs.xpAwards` action its batches pay (`m_sXpAction`); a furnace, smelter or refinery row says
//! `smelt_batch`, so a worker who smelts climbs SMELTING and never MINER. A row with no action pays
//! nothing. Called once per finished run, instant or timed, with the batches that run covered.
//! Server only.
class LF_ProcessXp
{
	static const string FEATURE = "Processing";

	//------------------------------------------------------------------------------------------------
	//! Pays \p playerUid for \p batches finished at the machine \p machineId.
	//! \return XP actually added; 0 when the machine pays none, the worker is no player, or the award is refused.
	static int PayBatches(string playerUid, string machineId, int batches)
	{
		if (playerUid.IsEmpty() || batches < 1)
			return 0;

		LF_ResourceMachineDef machine = LF_ResourceChainCatalog.FindMachine(machineId);
		if (!machine || machine.m_sXpAction.IsEmpty())
			return 0;

		int paid;
		if (machine.m_sXpAction == LF_JobLevelSources.ACTION_SMELT_BATCH)
			paid = LF_JobLevelSources.AwardSmelting(playerUid, batches);
		else if (machine.m_sXpAction == LF_JobLevelSources.ACTION_SALVAGE_WORK)
			paid = LF_JobLevelSources.AwardSalvage(playerUid, batches);
		else
			paid = LF_JobLevels.Award(playerUid, machine.m_sXpAction, batches);

		LF_Debug.Info(FEATURE, string.Format("%1 batch(es) at %2 paid %3 xp (%4)", batches, machineId, paid, machine.m_sXpAction));
		return paid;
	}

	//------------------------------------------------------------------------------------------------
	//! Same, for the worker's body (\p user may be null or no player: nothing is paid).
	static int PayBatches(IEntity user, string machineId, int batches)
	{
		if (!user)
			return 0;

		return PayBatches(LF_Utils.GetPlayerUID(user), machineId, batches);
	}
}
