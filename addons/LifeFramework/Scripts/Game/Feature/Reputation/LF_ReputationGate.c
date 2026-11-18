//------------------------------------------------------------------------------------------------
//! One standing gate row (#2701, docs/architecture/reputation.md section 4): the faction whose tier
//! a job, a run or (later) an outlet or contract reads, and the lowest tier that passes. A gate id
//! with no row is open. The core ships the rows in Configs/Reputation/LF_Reputation.conf; a pack
//! overlay retunes them.
[BaseContainerProps()]
class LF_ReputationGateDef : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Gate id the game checks, e.g. job:TAXI, run:TAXI. The record key")]
	string m_sGate;

	[Attribute("", UIWidgets.EditBox, "Faction id whose tier is read")]
	string m_sFaction;

	[Attribute("0", UIWidgets.Auto, "Lowest tier that passes (-2..3 with the shipped ladder)", params: "-10 10")]
	int m_iMinTier;

	//------------------------------------------------------------------------------------------------
	static LF_ReputationGateDef Make(string gate, string faction, int minTier)
	{
		LF_ReputationGateDef def = new LF_ReputationGateDef();
		def.m_sGate = gate;
		def.m_sFaction = faction;
		def.m_iMinTier = minTier;
		return def;
	}
}

//------------------------------------------------------------------------------------------------
//! The standing gate check on LF_Reputation (#2701). Server-side: the points live on the server, so
//! a client offer cannot read them yet (the owner RPC is a later card of #1018) and the server
//! re-checks every use. A refusal is a `#` key that names the faction, plus the faction's tier
//! word for the minimum tier as the argument; never a number or a points value.
modded class LF_Reputation
{
	//------------------------------------------------------------------------------------------------
	//! \return The gate's row from the loaded config, or null (no config, or no row for the id).
	static LF_ReputationGateDef FindGate(string gateId)
	{
		if (gateId.IsEmpty())
			return null;

		LF_ReputationConfig config = LF_ReputationConfig.Get();
		if (!config || !config.m_aGates)
			return null;

		foreach (LF_ReputationGateDef gate : config.m_aGates)
		{
			if (gate && gate.m_sGate == gateId)
				return gate;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the character's tier with the gate's faction reaches the gate's minimum. A
	//! gate id with no row passes; a row naming a faction the config does not list also passes (the
	//! typo is logged once per call, never locks a job).
	static bool Passes(string playerUid, string gateId)
	{
		LF_ReputationGateDef gate = FindGate(gateId);
		if (!gate)
			return true;

		if (!FactionDef(gate.m_sFaction))
		{
			LF_Debug.Warn(FEATURE, string.Format("gate '%1' names faction '%2' which is not in the reputation config; it passes", gateId, gate.m_sFaction));
			return true;
		}

		return Tier(playerUid, gate.m_sFaction) >= gate.m_iMinTier;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[out] tierWord The `#` key of the minimum tier's word in the faction's ladder (the
	//! notification argument); empty when the faction is not shown to players.
	//! \return "" when the character passes, else the `#` key that refuses in the faction's words:
	//! the faction name is part of the key and no number is.
	static string Refusal(string playerUid, string gateId, out string tierWord)
	{
		tierWord = string.Empty;
		if (Passes(playerUid, gateId))
			return string.Empty;

		LF_ReputationGateDef gate = FindGate(gateId);
		LF_ReputationFactionDef def = FactionDef(gate.m_sFaction);
		if (def.m_bShown)
			tierWord = TierWordKey(def, gate.m_iMinTier);

		return RefusalKey(gate.m_sFaction);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The refusal key for a faction; the generic one for a faction without its own words.
	static string RefusalKey(string faction)
	{
		if (faction == FACTION_TOWN)
			return "#LF-Reputation_Refuse_Town";
		if (faction == FACTION_POLICE)
			return "#LF-Reputation_Refuse_Police";
		if (faction == FACTION_UNDERWORLD)
			return "#LF-Reputation_Refuse_Underworld";

		return "#LF-Reputation_Refuse_Any";
	}

	//------------------------------------------------------------------------------------------------
	//! \return The `#` key of tier `tier`'s word in the faction's ladder, empty when the table has none.
	static string TierWordKey(notnull LF_ReputationFactionDef def, int tier)
	{
		if (!def.m_aTierLabels)
			return string.Empty;

		int index = tier - LF_ReputationFactionDef.FIRST_TIER;
		if (index < 0 || index >= def.m_aTierLabels.Count())
			return string.Empty;

		return def.m_aTierLabels[index];
	}

	//------------------------------------------------------------------------------------------------
	//! \return The refusal as player text (a greyed action's reason): the key's words with the tier word.
	static string RefusalText(string refusalKey, string tierWord)
	{
		if (tierWord.IsEmpty())
			return LF_Terms.Text(refusalKey);

		return LF_Terms.Text(refusalKey, LF_Terms.Text(tierWord));
	}
}
