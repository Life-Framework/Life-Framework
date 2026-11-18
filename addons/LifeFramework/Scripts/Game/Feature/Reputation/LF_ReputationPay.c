//------------------------------------------------------------------------------------------------
//! One faction's standing pay row (#2703, docs/architecture/reputation.md sections 3 and 8): the
//! percent a respected worker earns on top of that faction's jobs, one entry per tier, lowest tier
//! first (the shipped ladder is -2..3, so `0 0 0 5 10 15`). A faction with no row pays no bonus.
[BaseContainerProps()]
class LF_ReputationPayDef : LF_ConfigRecord
{
	//! The most percent any tier may pay; a config above it clamps here (the economy sizing of section 8).
	static const int MAX_PERCENT = 15;	// lf-fixed: the design ceiling, docs/architecture/reputation.md section 8

	[Attribute("", UIWidgets.EditBox, "Faction id whose jobs get the bonus. The record key")]
	string m_sFaction;

	[Attribute("", UIWidgets.Auto, "Bonus percent per tier, lowest tier first: 0 0 0 5 10 15 are tiers -2..3. Capped at 15")]
	ref array<int> m_aPayPercentByTier;

	//------------------------------------------------------------------------------------------------
	static LF_ReputationPayDef Make(string faction, notnull array<int> percents)
	{
		LF_ReputationPayDef def = new LF_ReputationPayDef();
		def.m_sFaction = faction;
		def.m_aPayPercentByTier = {};
		def.m_aPayPercentByTier.Copy(percents);
		return def;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The bonus percent for `tier`, 0..MAX_PERCENT; 0 for a tier the table does not list.
	int PercentFor(int tier)
	{
		if (!m_aPayPercentByTier)
			return 0;

		int index = tier - LF_ReputationFactionDef.FIRST_TIER;
		if (index < 0 || index >= m_aPayPercentByTier.Count())
			return 0;

		return LF_Utils.ClampInt(m_aPayPercentByTier[index], 0, MAX_PERCENT);
	}
}

//------------------------------------------------------------------------------------------------
//! The standing pay bonus on civilian runs (#2703). Server-side, called by LF_CivilianJob.CompleteRun
//! with the run's BASE pay: the bonus is a percent of that base only (a contract's multiplier does not
//! compound it) and reaches the player as its own ledger line, reason `standing-bonus`, so its effect
//! on the money supply can be measured. Police and medic duty pay never calls this.
class LF_ReputationPay
{
	static const string REASON = "standing-bonus";

	protected static ref map<string, int> s_mLastBonus;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mLastBonus = null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The faction whose standing pays on civilian runs: the town.
	static string JobFaction()
	{
		return LF_Reputation.FACTION_TOWN;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The pay row of a faction in the loaded config, or null.
	static LF_ReputationPayDef FindDef(string faction)
	{
		LF_ReputationConfig config = LF_ReputationConfig.Get();
		if (!config || !config.m_aPays)
			return null;

		foreach (LF_ReputationPayDef def : config.m_aPays)
		{
			if (def && def.m_sFaction == faction)
				return def;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The bonus percent the character earns on `faction`'s jobs at their current tier; 0 with
	//! no pay row.
	static int Percent(string playerUid, string faction)
	{
		LF_ReputationPayDef def = FindDef(faction);
		if (!def)
			return 0;

		return def.PercentFor(LF_Reputation.Tier(playerUid, faction));
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from LF_CivilianJob.CompleteRun: the bonus on a finished run's base pay. The amount is
	//! remembered for TakeLastBonus, which the drop-off hands over as its own ledger line.
	//! \return The bonus money (0 when the character has no bonus tier or the base is not positive).
	static int Bonus(string playerUid, int basePay)
	{
		if (s_mLastBonus)
			s_mLastBonus.Remove(playerUid);

		if (playerUid.IsEmpty() || basePay <= 0)
			return 0;

		int percent = Percent(playerUid, JobFaction());
		if (percent <= 0)
			return 0;

		int bonus = Math.Round(basePay * percent / 100.0); // lf-float: a percent of a small base, far below 2^24
		if (bonus <= 0)
			return 0;

		if (!s_mLastBonus)
			s_mLastBonus = new map<string, int>();

		s_mLastBonus.Set(playerUid, bonus);
		LF_Debug.Info(LF_Reputation.FEATURE, string.Format("standing bonus for %1: +%2 (%3 percent of base %4)", playerUid, bonus, percent, basePay));
		return bonus;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The standing bonus of the character's last Bonus call, and clears it.
	static int TakeLastBonus(string playerUid)
	{
		int bonus;
		if (!s_mLastBonus || !s_mLastBonus.Find(playerUid, bonus))
			return 0;

		s_mLastBonus.Remove(playerUid);
		return bonus;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the character's pending bonus (career wipe, test cleanup).
	static void Forget(string playerUid)
	{
		if (s_mLastBonus)
			s_mLastBonus.Remove(playerUid);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ReputationPay's statics.
class LF_ReputationPay_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ReputationPay";
	}

	override void Apply()
	{
		LF_ReputationPay.ResetStatics();
	}
}
