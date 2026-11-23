//------------------------------------------------------------------------------------------------
//! Outpost standing (0.3.0 gangs O4, decision N27). A sale at an outpost trader earns standing on the
//! one underworld track (LF_Reputation, faction UNDERWORLD, the "fence_sale" award row: its points and
//! cooldown are config) and marks the character "known here" at that outpost. There is no
//! per-outpost standing to grind: the flag sits on top of the one track.
//!
//! The trader sale (LF_TraderSale.Sell) and the shop till (LF_CharacterShopComponent.ServeTrade) call
//! OnSale after a completed trade. A trade outside every outpost changes nothing here.
//!
//! The flags are keyed by the character record key (LF_Utils.GetPlayerUID) and the outpost id (the
//! part of the zone name after "outpost:"), never an entity, and saved in the "outpoststanding"
//! section (LF_OutpostStandingSavePayload below).
//------------------------------------------------------------------------------------------------
class LF_OutpostStanding
{
	static const string FEATURE = "Outposts";
	//! The award row an outpost sale books on the underworld track.
	static const string AWARD_ACTION = "fence_sale";

	//! "uid|outpostId" -> known. Server state.
	protected static ref map<string, bool> s_mKnown;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mKnown = null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The id of the registered outpost whose zone holds position; empty outside every outpost.
	static string OutpostAt(vector position)
	{
		array<ref LF_ZoneArea> zones = LF_Zones.GetAll();
		foreach (LF_ZoneArea zone : zones)
		{
			if (zone.m_sName.StartsWith(LF_Outpost.ZONE_PREFIX) && zone.Contains(position))
				return zone.m_sName.Substring(LF_Outpost.ZONE_PREFIX.Length(), zone.m_sName.Length() - LF_Outpost.ZONE_PREFIX.Length());	// lf-bounded: a zone name
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether the character is known at the outpost.
	static bool IsKnownHere(string playerUid, string outpostId)
	{
		if (!s_mKnown || playerUid.IsEmpty() || outpostId.IsEmpty())
			return false;

		return s_mKnown.Get(Key(playerUid, outpostId));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a completed trade by customer at the trader at traderPos. Inside an outpost it books the
	//! underworld award and the known-here flag.
	//! \return The outpost id the sale counted at; empty when it was not at an outpost or was refused.
	static string OnSale(IEntity customer, vector traderPos)
	{
		if (Replication.IsRunning() && !Replication.IsServer())
			return string.Empty;

		string outpostId = OutpostAt(traderPos);
		if (outpostId.IsEmpty())
			return string.Empty;

		string uid = LF_Utils.GetPlayerUID(customer);
		if (uid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("outpost %1 sale skipped: the customer has no record key", outpostId));
			return string.Empty;
		}

		RecordSale(uid, outpostId);
		return outpostId;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: books one outpost sale for a character key.
	//! \return The underworld points that moved (0 on cooldown or at the daily cap; the flag is set anyway).
	static int RecordSale(string playerUid, string outpostId)
	{
		if (playerUid.IsEmpty() || outpostId.IsEmpty())
		{
			LF_Debug.Error(FEATURE, string.Format("outpost sale refused: uid '%1' outpost '%2'", playerUid, outpostId));
			return 0;
		}

		int moved = LF_Reputation.Award(playerUid, AWARD_ACTION);
		if (MarkKnown(playerUid, outpostId))
			LF_Debug.Info(FEATURE, string.Format("%1 is now known at outpost %2", playerUid, outpostId));

		LF_Debug.Info(FEATURE, string.Format("outpost %1 sale by %2: underworld %3", outpostId, playerUid, moved));
		return moved;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the flag was not set before.
	protected static bool MarkKnown(string playerUid, string outpostId)
	{
		if (!s_mKnown)
			s_mKnown = new map<string, bool>();

		string key = Key(playerUid, outpostId);
		if (s_mKnown.Get(key))
			return false;

		s_mKnown.Set(key, true);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Key(string playerUid, string outpostId)
	{
		return playerUid + "|" + outpostId;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops one character's flags (test cleanup, admin wipe).
	static void Forget(string playerUid)
	{
		if (!s_mKnown)
			return;

		array<string> keys = {};
		foreach (string key, bool known : s_mKnown)
		{
			if (key.StartsWith(playerUid + "|"))
				keys.Insert(key);
		}

		foreach (string key : keys)
		{
			s_mKnown.Remove(key);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: one row per known-here flag.
	static array<ref LF_OutpostKnownRow> ExportAll()
	{
		array<ref LF_OutpostKnownRow> rows = new array<ref LF_OutpostKnownRow>();
		if (!s_mKnown)
			return rows;

		foreach (string key, bool known : s_mKnown)
		{
			int bar = key.LastIndexOf("|");
			if (!known || bar <= 0)
				continue;

			rows.Insert(LF_OutpostKnownRow.Create(key.Substring(0, bar), key.Substring(bar + 1, key.Length() - bar - 1)));	// lf-bounded: a uid and an outpost id
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: replaces the flags with the saved rows (idempotent).
	//! \return Rows applied.
	static int ApplyAll(array<ref LF_OutpostKnownRow> rows)
	{
		s_mKnown = new map<string, bool>();
		if (!rows)
			return 0;

		int applied = 0;
		foreach (LF_OutpostKnownRow row : rows)
		{
			if (!row || row.m_sCharacterKey.IsEmpty() || row.m_sOutpostId.IsEmpty())
			{
				LF_Debug.Warn(FEATURE, "load skipped a known-here row with no character or no outpost");
				continue;
			}

			s_mKnown.Set(Key(row.m_sCharacterKey, row.m_sOutpostId), true);
			applied++;
		}

		return applied;
	}
}

//------------------------------------------------------------------------------------------------
//! One known-here flag in the "outpoststanding" save section. Never reorder; a new shape raises the
//! section's Version().
class LF_OutpostKnownRow
{
	string m_sCharacterKey;
	string m_sOutpostId;

	//------------------------------------------------------------------------------------------------
	static LF_OutpostKnownRow Create(string characterKey, string outpostId)
	{
		LF_OutpostKnownRow row = new LF_OutpostKnownRow();
		row.m_sCharacterKey = characterKey;
		row.m_sOutpostId = outpostId;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! Save section "outpoststanding" (#4224): the known-here flags. The underworld points themselves are
//! in the reputation payload. OnAbsent keeps the default: a save from before the section knows nobody.
class LF_OutpostStandingSavePayload : LF_SavePayload
{
	static const string NAME = "outpoststanding";

	//------------------------------------------------------------------------------------------------
	override string Name()
	{
		return NAME;
	}

	//------------------------------------------------------------------------------------------------
	override int Version()
	{
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	override void WriteSection(notnull SaveContext context)
	{
		array<ref LF_OutpostKnownRow> rows = LF_OutpostStanding.ExportAll();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 outpost known-here row(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_OutpostKnownRow> rows = new array<ref LF_OutpostKnownRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_OutpostStanding.FEATURE, "outpost known-here rows failed to read; the live flags stay as they were");
			return false;
		}

		int applied = LF_OutpostStanding.ApplyAll(rows);
		LF_Debug.Info("Persistence", string.Format("applied %1 of %2 outpost known-here row(s)", applied, rows.Count()));
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_OutpostStanding's statics (LF_StaticReset finds it by reflection).
class LF_OutpostStanding_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_OutpostStanding";
	}

	override void Apply()
	{
		LF_OutpostStanding.ResetStatics();
	}
}
