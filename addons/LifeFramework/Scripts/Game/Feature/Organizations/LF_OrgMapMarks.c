//------------------------------------------------------------------------------------------------
//! One saved org mark, a row of the "orgmarks" payload (card M12, #2240). Keyed by organization id
//! and a serial the organization never reuses, never by an entity id. Positions are world metres.
class LF_OrgMapMarkRow
{
	int m_iVersion = 1;
	string m_sOrgId;
	int m_iSerial;
	int m_iX;
	int m_iZ;
	int m_iIcon;
	int m_iColour;
	string m_sText;
	//! The character who placed it (for logs and moderation).
	string m_sAuthorId;

	//------------------------------------------------------------------------------------------------
	static LF_OrgMapMarkRow Make(string orgId, int serial, int x, int z, int icon, int colour, string text, string authorId)
	{
		LF_OrgMapMarkRow row = new LF_OrgMapMarkRow();
		row.m_sOrgId = orgId;
		row.m_iSerial = serial;
		row.m_iX = x;
		row.m_iZ = z;
		row.m_iIcon = icon;
		row.m_iColour = colour;
		row.m_sText = text;
		row.m_sAuthorId = authorId;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! Organisation map marks (card M12, everon-map.md section 3; D-M5, D-M11, D-M12). A member whose rank
//! holds the territory right places a mark for the whole organization, at most CAP per organization.
//! The marks reach the members only, through LF_MapShares' ORG audience (`org:<orgId>:<serial>`), and
//! are saved as "orgmarks", so they survive restarts and the 24 h world reset. This is how an org
//! shares an underworld place by hand: the server never makes one public (D-M5).
//! Server only. A client asks through LF_MapShareComponent.AskOrgMark / AskOrgMarkRemove.
class LF_OrgMapMarks
{
	static const string FEATURE = "OrgMapMarks";
	//! The rank right that may add and remove org marks (reserved for territory until now).
	static const LF_EOrganizationPermission RIGHT = LF_EOrganizationPermission.MANAGE_TERRITORY;
	static const int CAP = 20; // lf-fixed: D-M11, 20 marks per organization
	static const float RATE_SECONDS = 2; // lf-fixed: anti-spam floor between one character's adds
	static const int WIRE_MIN = -32768; // lf-fixed: positions travel as int16 metres (everon-map.md section 3)
	static const int WIRE_MAX = 32767; // lf-fixed: positions travel as int16 metres
	static const int INDEX_MAX = 255; // lf-fixed: icon and colour travel as u8

	protected static ref array<ref LF_OrgMapMarkRow> s_aRows;
	//! World seconds of each character's last add.
	protected static ref map<string, float> s_mLastAddAt;
	//! Highest serial each organization has used this session, so a removed key is never handed out
	//! again while a client may still hold it.
	protected static ref map<string, int> s_mHighSerial;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aRows = null;
		s_mLastAddAt = null;
		s_mHighSerial = null;
	}

	//------------------------------------------------------------------------------------------------
	static string KeyOf(string orgId, int serial)
	{
		return string.Format("org:%1:%2", orgId, serial);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a member adds a mark for their organization. \return the share key, or "" when refused
	//! (logged): no organization, no right, the cap, the rate limit, or a value outside the wire range.
	static string Add(string characterId, int x, int z, int icon, int colour, string text, float now)
	{
		if (!Replication.IsServer())
		{
			LF_Debug.Error(FEATURE, "add refused: not the server");
			return "";
		}

		LF_Organization org = LF_OrganizationManager.GetByCharacter(characterId);
		if (!org)
		{
			LF_Debug.Warn(FEATURE, string.Format("add refused: %1 belongs to no organization", characterId));
			return "";
		}

		string orgId = org.GetId();
		if (!CanUseOrgMarks(org, characterId))
		{
			LF_Debug.Warn(FEATURE, string.Format("add refused: %1 lacks the territory right in %2", characterId, orgId));
			return "";
		}

		if (x < WIRE_MIN || x > WIRE_MAX || z < WIRE_MIN || z > WIRE_MAX || icon < 0 || icon > INDEX_MAX || colour < 0 || colour > INDEX_MAX)
		{
			LF_Debug.Warn(FEATURE, string.Format("add refused: %1 sent a value outside the wire range (%2,%3 icon %4 colour %5)", characterId, x, z, icon, colour));
			return "";
		}

		if (CountOf(orgId) >= CAP)
		{
			LF_Debug.Warn(FEATURE, string.Format("add refused: %1 already holds %2 marks", orgId, CAP));
			return "";
		}

		EnsureTables();
		if (OnCooldown(characterId, now))
		{
			LF_Debug.Warn(FEATURE, string.Format("add refused: %1 adds too fast", characterId));
			return "";
		}

		LF_OrgMapMarkRow row = LF_OrgMapMarkRow.Make(orgId, NextSerial(orgId), x, z, icon, colour, CleanText(text), characterId);
		s_aRows.Insert(row);
		s_mLastAddAt.Set(characterId, now);
		s_mHighSerial.Set(orgId, row.m_iSerial);
		string key = KeyOf(orgId, row.m_iSerial);
		LF_MapShares.Put(ToShare(row));
		LF_Debug.Info(FEATURE, string.Format("mark %1 added by %2 (%3 of %4)", key, characterId, CountOf(orgId), CAP));
		return key;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a member with the right removes one of their organization's marks. \return false when refused.
	static bool Remove(string characterId, string key)
	{
		if (!Replication.IsServer())
			return false;

		LF_Organization org = LF_OrganizationManager.GetByCharacter(characterId);
		if (!org || !CanUseOrgMarks(org, characterId))
		{
			LF_Debug.Warn(FEATURE, string.Format("remove refused: %1 lacks the territory right", characterId));
			return false;
		}

		int index = IndexOf(key);
		if (index == -1 || s_aRows[index].m_sOrgId != org.GetId())
		{
			LF_Debug.Warn(FEATURE, string.Format("remove refused: %1 is not a mark of %2", key, org.GetId()));
			return false;
		}

		s_aRows.RemoveOrdered(index);
		LF_MapShares.Remove(key);
		LF_Debug.Info(FEATURE, string.Format("mark %1 removed by %2", key, characterId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static int CountOf(string orgId)
	{
		if (!s_aRows)
			return 0;

		int count;
		foreach (LF_OrgMapMarkRow row : s_aRows)
		{
			if (row.m_sOrgId == orgId)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	static bool Has(string key)
	{
		return IndexOf(key) != -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every mark, copied, for the "orgmarks" payload.
	static array<ref LF_OrgMapMarkRow> ExportAll()
	{
		array<ref LF_OrgMapMarkRow> rows = new array<ref LF_OrgMapMarkRow>();
		if (!s_aRows)
			return rows;

		foreach (LF_OrgMapMarkRow row : s_aRows)
		{
			LF_OrgMapMarkRow copy = LF_OrgMapMarkRow.Make(row.m_sOrgId, row.m_iSerial, row.m_iX, row.m_iZ, row.m_iIcon, row.m_iColour, row.m_sText, row.m_sAuthorId);
			rows.Insert(copy);
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Server load: replaces every mark with \rows and re-puts them into LF_MapShares (which saves
	//! nothing). Idempotent. Skips a row with no organization or one that no longer exists (organizations
	//! load first), a bad serial, a repeated key, or one past the cap. Null clears. \return how many rows were applied.
	static int ApplyAll(array<ref LF_OrgMapMarkRow> rows)
	{
		if (s_aRows)
		{
			foreach (LF_OrgMapMarkRow old : s_aRows)
			{
				LF_MapShares.Remove(KeyOf(old.m_sOrgId, old.m_iSerial));
			}
		}

		s_aRows = new array<ref LF_OrgMapMarkRow>();
		if (!rows)
			return 0;

		int applied;
		foreach (LF_OrgMapMarkRow row : rows)
		{
			if (!row || row.m_sOrgId.IsEmpty() || row.m_iSerial <= 0)
			{
				LF_Debug.Warn(FEATURE, "load skipped an org mark with no organization or serial");
				continue;
			}

			if (!LF_OrganizationManager.Get(row.m_sOrgId))
			{
				LF_Debug.Warn(FEATURE, string.Format("load dropped mark %1:%2: no such organization", row.m_sOrgId, row.m_iSerial));
				continue;
			}

			string key = KeyOf(row.m_sOrgId, row.m_iSerial);
			if (Has(key))
			{
				LF_Debug.Warn(FEATURE, string.Format("load skipped repeated mark %1", key));
				continue;
			}

			if (CountOf(row.m_sOrgId) >= CAP)
			{
				LF_Debug.Warn(FEATURE, string.Format("load skipped mark %1: %2 is at the cap", key, row.m_sOrgId));
				continue;
			}

			LF_OrgMapMarkRow copy = LF_OrgMapMarkRow.Make(row.m_sOrgId, row.m_iSerial, row.m_iX, row.m_iZ, row.m_iIcon, row.m_iColour, CleanText(row.m_sText), row.m_sAuthorId);
			s_aRows.Insert(copy);
			LF_MapShares.Put(ToShare(copy));
			applied++;
		}

		return applied;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a character joined or left an organization. Their map gets or loses the org's marks at
	//! once, without waiting for the share sweep. A character who is not online is skipped.
	static void RefreshCharacter(string characterId)
	{
		if (!Replication.IsServer() || characterId.IsEmpty() || !GetGame())
			return;

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return;

		array<int> ids = {};
		players.GetPlayers(ids);
		foreach (int playerId : ids)
		{
			if (LF_Utils.GetPlayerUID(playerId) == characterId)
			{
				LF_MapShares.Refresh(playerId);
				return;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Trimmed, without leading '#' (the client would translate it as a string table key), and cut to the
	//! share text cap. No profanity filter exists in the core yet.
	static string CleanText(string text)
	{
		string clean = text.Trim();
		while (clean.StartsWith("#"))
		{
			clean = clean.Substring(1, clean.Length() - 1).Trim();
		}

		if (clean.Length() > LF_MapShares.TEXT_CAP)
			clean = clean.Substring(0, LF_MapShares.TEXT_CAP);

		return clean;
	}

	//------------------------------------------------------------------------------------------------
	//! The rank right check for adding and removing the organization's marks.
	static bool CanUseOrgMarks(notnull LF_Organization org, string characterId)
	{
		return org.Allows(characterId, RIGHT);
	}

	//------------------------------------------------------------------------------------------------
	//! One add per RATE_SECONDS per character.
	protected static bool OnCooldown(string characterId, float now)
	{
		return s_mLastAddAt && s_mLastAddAt.Contains(characterId) && now - s_mLastAddAt.Get(characterId) < RATE_SECONDS;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_MapShareRow ToShare(notnull LF_OrgMapMarkRow row)
	{
		return LF_MapShareRow.Make(KeyOf(row.m_sOrgId, row.m_iSerial), LF_EMapShareAudience.ORG, row.m_sOrgId, row.m_iX, row.m_iZ, row.m_iIcon, row.m_iColour, row.m_sText, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! One past the highest serial the organization holds or has used this session, so a removed mark's
	//! key is not reused while a client may still hold it (a spawn resend clears the client's set).
	protected static int NextSerial(string orgId)
	{
		int highest;
		if (s_mHighSerial)
			highest = s_mHighSerial.Get(orgId);

		foreach (LF_OrgMapMarkRow row : s_aRows)
		{
			if (row.m_sOrgId == orgId && row.m_iSerial > highest)
				highest = row.m_iSerial;
		}

		return highest + 1;
	}

	//------------------------------------------------------------------------------------------------
	protected static int IndexOf(string key)
	{
		if (!s_aRows)
			return -1;

		foreach (int index, LF_OrgMapMarkRow row : s_aRows)
		{
			if (KeyOf(row.m_sOrgId, row.m_iSerial) == key)
				return index;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected static void EnsureTables()
	{
		if (!s_aRows)
			s_aRows = new array<ref LF_OrgMapMarkRow>();

		if (!s_mLastAddAt)
			s_mLastAddAt = new map<string, float>();

		if (!s_mHighSerial)
			s_mHighSerial = new map<string, int>();
	}
}

//------------------------------------------------------------------------------------------------
//! The client's ask for an org mark goes up the player's own share channel; the server checks the
//! organization, the rank right, the cap and the rate limit (LF_OrgMapMarks). A client never puts a
//! shared row itself.
modded class LF_MapShareComponent
{
	//------------------------------------------------------------------------------------------------
	//! Client: ask to mark this spot for the organization.
	void AskOrgMark(int x, int z, int icon, int colour, string text)
	{
		if (Replication.IsServer())
			RpcAsk_OrgMark(x, z, icon, colour, text);
		else
			Rpc(RpcAsk_OrgMark, x, z, icon, colour, text);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: ask to remove one of the organization's marks.
	void AskOrgMarkRemove(string key)
	{
		if (Replication.IsServer())
			RpcAsk_OrgMarkRemove(key);
		else
			Rpc(RpcAsk_OrgMarkRemove, key);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_OrgMark(int x, int z, int icon, int colour, string text)
	{
		string characterId = OwnerCharacterId();
		if (characterId.IsEmpty())
		{
			LF_Debug.Warn(LF_OrgMapMarks.FEATURE, "org mark ask refused: the asking player has no character");
			return;
		}

		LF_OrgMapMarks.Add(characterId, x, z, icon, colour, text, LF_MapShares.Now());
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_OrgMarkRemove(string key)
	{
		string characterId = OwnerCharacterId();
		if (characterId.IsEmpty())
		{
			LF_Debug.Warn(LF_OrgMapMarks.FEATURE, "org mark remove refused: the asking player has no character");
			return;
		}

		LF_OrgMapMarks.Remove(characterId, key);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the character key of the player who owns this controller, "" when unknown.
	protected string OwnerCharacterId()
	{
		PlayerController controller = PlayerController.Cast(GetOwner());
		if (!controller)
			return "";

		return LF_Utils.GetPlayerUID(controller.GetPlayerId());
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_OrgMapMarks' statics (registered at script load; Core/LF_StaticReset.c).
class LF_OrgMapMarks_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_OrgMapMarks_StaticResetHook());

	override string Owner()
	{
		return "LF_OrgMapMarks";
	}

	override void Apply()
	{
		LF_OrgMapMarks.ResetStatics();
	}
}
