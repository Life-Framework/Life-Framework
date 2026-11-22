//------------------------------------------------------------------------------------------------
//! One member's position for a live-marks pass (card C10, #4216). Server side; built from the game by
//! LF_OrgLiveMarks.Tick, or by a test.
class LF_OrgLiveFix : Managed
{
	//! The id organizations list the member under (LF_OrgMapMarks.CharacterIdOfPlayer).
	string m_sCharacterId;
	//! World x and z in metres.
	int m_iX;
	int m_iZ;
	//! Downed or in custody: the mark is drawn dim (N25).
	bool m_bDown;
	//! The member's name on the mark, cut to LF_MapShares.TEXT_CAP.
	string m_sName;

	//------------------------------------------------------------------------------------------------
	static LF_OrgLiveFix Make(string characterId, int x, int z, bool down, string name)
	{
		LF_OrgLiveFix fix = new LF_OrgLiveFix();
		fix.m_sCharacterId = characterId;
		fix.m_iX = x;
		fix.m_iZ = z;
		fix.m_bDown = down;
		fix.m_sName = name;
		return fix;
	}
}

//------------------------------------------------------------------------------------------------
//! What was last sent for one live mark, so a member standing still costs no traffic.
class LF_OrgLiveSent : Managed
{
	int m_iX;
	int m_iZ;
	bool m_bDown;
}

//------------------------------------------------------------------------------------------------
//! Live member marks (card C10, decision N25): every INTERVAL_MS the server puts one mark per online
//! organization member at their position, through LF_MapShares' ORG audience, so only that
//! organization's members receive it; nobody outside gets a byte. A downed or jailed member's mark
//! is dim. A member who goes offline, dies, has no body or leaves the organization loses their mark
//! on the next pass. A mark is re-sent only when the member moved MOVE_M or more or changed state.
//! Not the placed org marks of LF_OrgMapMarks (#2240): these are never saved.
//! Key: `orglive:<orgId>:<characterId>`.
//! Not yet: hiding the marks from a viewer who is in custody (N25) needs the share audience to know
//! custody (LF_MapShareGameFacts), a follow-up card.
class LF_OrgLiveMarks
{
	static const string FEATURE = "OrgLiveMarks";
	static const string KEY_PREFIX = "orglive:";
	static const int INTERVAL_MS = 5000; // lf-fixed: N25, live marks every 5 s
	static const int MOVE_M = 5; // lf-fixed: re-send floor, below the map's drawn size of a mark
	static const int ICON = 1; // lf-fixed: placed-marker icon index (pencil_circle)
	static const int COLOUR_LIVE = 5; // lf-fixed: placed-marker colour index (green)
	static const int COLOUR_DOWN = 12; // lf-fixed: placed-marker colour index (the darkest; the list has no grey yet)

	//! Live key to what was last sent under it.
	protected static ref map<string, ref LF_OrgLiveSent> s_mSent;
	protected static bool s_bRunning;
	protected static int s_iPuts;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bRunning && GetGame())
			GetGame().GetCallqueue().Remove(LF_OrgLiveMarks.Tick);

		s_mSent = null;
		s_bRunning = false;
		s_iPuts = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: start the pass timer. Safe to call again.
	static bool EnsureStarted()
	{
		if (!Replication.IsServer() || !GetGame())
			return false;

		if (!s_bRunning)
		{
			GetGame().GetCallqueue().CallLater(LF_OrgLiveMarks.Tick, INTERVAL_MS, true);
			s_bRunning = true;
			LF_Debug.Info(FEATURE, string.Format("live member marks every %1 ms", INTERVAL_MS));
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static string KeyOf(string orgId, string characterId)
	{
		return KEY_PREFIX + orgId + ":" + characterId;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, every INTERVAL_MS: read every online player's body and publish.
	static void Tick()
	{
		if (!Replication.IsServer() || !GetGame())
			return;

		array<ref LF_OrgLiveFix> fixes = {};
		Gather(fixes);
		Publish(fixes);
	}

	//------------------------------------------------------------------------------------------------
	//! One fix per online player with a character and a living body. A player with no account,
	//! no character or no body is left out, so their mark goes.
	static void Gather(notnull array<ref LF_OrgLiveFix> outFixes)
	{
		outFixes.Clear();
		PlayerManager players = GetGame().GetPlayerManager();
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!players || !accounts)
			return;

		array<int> ids = {};
		players.GetPlayers(ids);
		foreach (int playerId : ids)
		{
			LF_PlayerAccount account = accounts.GetAccount(playerId);
			string characterId = LF_OrgMapMarks.CharacterIdOfAccount(account);
			if (characterId.IsEmpty())
				continue;

			IEntity body = players.GetPlayerControlledEntity(playerId);
			if (!body)
				continue;

			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(body);
			if (character && character.GetCharacterController() && character.GetCharacterController().IsDead())
				continue;

			vector at = body.GetOrigin();
			bool down = LF_Downed.IsDowned(body) || LF_Jail.IsInCustody(LF_Utils.GetPlayerUID(playerId));
			string name = account.GetActiveCharacter().GetFullName();
			outFixes.Insert(LF_OrgLiveFix.Make(characterId, Math.Round(at[0]), Math.Round(at[2]), down, name));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: put a mark for every fix whose character is in an organization (ORG audience: that
	//! organization's members only) and remove every live mark from the last pass that has no fix now.
	//! A mark that moved less than MOVE_M and kept its state is not re-sent. \return marks put.
	static int Publish(notnull array<ref LF_OrgLiveFix> fixes)
	{
		if (!Replication.IsServer())
		{
			LF_Debug.Error(FEATURE, "publish refused: not the server");
			return 0;
		}

		if (!s_mSent)
			s_mSent = new map<string, ref LF_OrgLiveSent>();

		array<string> seen = {};
		int put;
		foreach (LF_OrgLiveFix fix : fixes)
		{
			if (!fix || fix.m_sCharacterId.IsEmpty())
				continue;

			LF_Organization org = LF_OrganizationManager.GetByCharacter(fix.m_sCharacterId);
			if (!org)
				continue;

			string orgId = org.GetId();
			string key = KeyOf(orgId, fix.m_sCharacterId);
			seen.Insert(key);
			LF_OrgLiveSent last = s_mSent.Get(key);
			if (last && last.m_bDown == fix.m_bDown && Math.AbsInt(last.m_iX - fix.m_iX) < MOVE_M && Math.AbsInt(last.m_iZ - fix.m_iZ) < MOVE_M)
				continue;

			int colour = COLOUR_LIVE;
			if (fix.m_bDown)
				colour = COLOUR_DOWN;

			string name = fix.m_sName.Trim();
			while (name.StartsWith("#"))
			{
				name = name.Substring(1, name.Length() - 1).Trim();
			}

			LF_MapShareRow row = LF_MapShareRow.Make(key, LF_EMapShareAudience.ORG, orgId, fix.m_iX, fix.m_iZ, ICON, colour, name, 0);
			if (!LF_MapShares.Put(row))
				continue;

			if (!last)
			{
				last = new LF_OrgLiveSent();
				s_mSent.Set(key, last);
			}

			last.m_iX = fix.m_iX;
			last.m_iZ = fix.m_iZ;
			last.m_bDown = fix.m_bDown;
			put++;
			s_iPuts++;
		}

		array<string> gone = {};
		foreach (string key, LF_OrgLiveSent sent : s_mSent)
		{
			if (seen.Find(key) == -1)
				gone.Insert(key);
		}

		foreach (string key : gone)
		{
			s_mSent.Remove(key);
			LF_MapShares.Remove(key);
			LF_Debug.Info(FEATURE, string.Format("live mark %1 removed", key));
		}

		return put;
	}

	//------------------------------------------------------------------------------------------------
	//! How many live marks the last passes hold.
	static int Count()
	{
		if (!s_mSent)
			return 0;

		return s_mSent.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Puts since the last reset (a test seam).
	static int Puts()
	{
		return s_iPuts;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsRunning()
	{
		return s_bRunning;
	}
}

//------------------------------------------------------------------------------------------------
//! Self-start: the live marks run from the world's start, on the server only.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_OrgLiveMarks.EnsureStarted();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_OrgLiveMarks' statics (LF_StaticReset finds it by reflection).
class LF_OrgLiveMarks_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_OrgLiveMarks";
	}

	override void Apply()
	{
		LF_OrgLiveMarks.ResetStatics();
	}
}
