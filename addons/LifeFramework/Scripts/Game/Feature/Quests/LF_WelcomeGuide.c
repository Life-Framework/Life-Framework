// LF_WelcomeGuide.c: the welcome guide's server state and rules. A brand-new character's first spawn
// (LF_SpawnLogic.OnFirstSpawn) marks the guide pending for that character and tells its player's client
// to open the sheet (LF_WelcomeGuideSheet). It stays pending, and opens again at the next join, until the
// player presses "Don't show again". The pending set is saved as the "welcomeguide" section, keyed by
// the character's persistence id. The server never draws anything; the client asks, the server answers.

//------------------------------------------------------------------------------------------------
//! One character whose welcome guide still opens by itself. Persisted as is.
class LF_WelcomeGuideRow
{
	string m_sCharacterId;

	//------------------------------------------------------------------------------------------------
	static LF_WelcomeGuideRow Create(string characterId)
	{
		LF_WelcomeGuideRow row = new LF_WelcomeGuideRow();
		row.m_sCharacterId = characterId;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	LF_WelcomeGuideRow Copy()
	{
		return Create(m_sCharacterId);
	}
}

//------------------------------------------------------------------------------------------------
//! What the "Set waypoint" button did, as the client's toast says it.
enum LF_EWelcomeWaypoint
{
	SET,
	//! Every guided arrival step is turned in: no errand mark to put up.
	NO_STEP,
	//! No body, or the body carries no quest tracker.
	NO_BODY,
	REFUSED
}

//------------------------------------------------------------------------------------------------
class LF_WelcomeGuide
{
	static const string FEATURE = "WelcomeGuide";
	//! The input action that opens the sheet again (chimeraInputCommon.conf, IngameContext).
	static const string INPUT_ACTION = "LF_WelcomeGuide";

	protected static ref array<string> s_aPending;

	//------------------------------------------------------------------------------------------------
	protected static array<string> Pending()
	{
		if (!s_aPending)
			s_aPending = new array<string>();

		return s_aPending;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aPending = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the guide opens by itself for this character from now on. Marking twice is one mark.
	//! \return False when the id is empty and nothing was marked.
	static bool MarkPending(string characterId)
	{
		if (characterId.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "guide not marked: the new character has no id");
			return false;
		}

		if (Pending().Find(characterId) != -1)
			return true;

		Pending().Insert(characterId);
		LF_Debug.Info(FEATURE, "guide pending for new character " + characterId);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while the guide still opens by itself for this character.
	static bool IsPending(string characterId)
	{
		return !characterId.IsEmpty() && Pending().Find(characterId) != -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, at a join: whether the guide opens by itself for this character. A character who has
	//! their papers (the clerk handed over the ID card) is past what page one tells a newcomer, so a
	//! mark still pending from their first spawn is dropped instead (review B6: a rejoin reopened "you
	//! carry no papers"). F3 still opens it.
	static bool ResolveAutoShow(string characterId, bool registered)
	{
		if (!IsPending(characterId))
			return false;

		if (!registered)
			return true;

		LF_Debug.Info(FEATURE, "guide not reopened for " + characterId + ": the character has their papers");
		Dismiss(characterId);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: "Don't show again". \return True when a pending mark was removed.
	static bool Dismiss(string characterId)
	{
		int idx = -1;
		if (!characterId.IsEmpty())
			idx = Pending().Find(characterId);

		if (idx == -1)
		{
			LF_Debug.Info(FEATURE, "dismiss for " + characterId + ": the guide was not pending");
			return false;
		}

		Pending().Remove(idx);
		LF_Debug.Info(FEATURE, "guide dismissed for good by " + characterId);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		return Pending().Count();
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_WelcomeGuideRow> ExportAll()
	{
		array<ref LF_WelcomeGuideRow> rows = new array<ref LF_WelcomeGuideRow>();
		foreach (string characterId : Pending())
		{
			rows.Insert(LF_WelcomeGuideRow.Create(characterId));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the pending set (idempotent). Rows with no id and repeats are skipped.
	//! \return Rows applied.
	static int ApplyAll(array<ref LF_WelcomeGuideRow> rows)
	{
		s_aPending = new array<string>();
		if (!rows)
			return 0;

		int applied = 0;
		foreach (LF_WelcomeGuideRow row : rows)
		{
			if (!row || row.m_sCharacterId.IsEmpty())
			{
				LF_Debug.Warn(FEATURE, "load skipped a welcome guide row with no character id");
				continue;
			}

			if (s_aPending.Find(row.m_sCharacterId) != -1)
				continue;

			s_aPending.Insert(row.m_sCharacterId);
			applied++;
		}

		return applied;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the active character's persistence id for a player, "" when unknown.
	static string CharacterIdOf(int playerId)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
			return string.Empty;

		LF_PlayerAccount account = accounts.GetAccount(playerId);
		if (!account || !account.GetActiveCharacter())
			return string.Empty;

		return account.GetActiveCharacter().GetId();
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from LF_SpawnLogic.OnFirstSpawn: marks the guide and tells the player's client to open it.
	static void OnFirstSpawn(int playerId, string characterId)
	{
		if (!MarkPending(characterId))
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!controller)
		{
			LF_Debug.Warn(FEATURE, "player " + playerId + " has no controller; the guide opens at their next join");
			return;
		}

		controller.LF_SendWelcomeGuideShow();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: what the "Set waypoint" button does. The arrival guide puts the character's current
	//! errand (the clerk, on a first spawn) on their map and screen; nothing new is decided here.
	//! \param[out] labelKey The mark's name, for the toast.
	static LF_EWelcomeWaypoint Waypoint(int playerId, out string labelKey)
	{
		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!body)
		{
			LF_Debug.Warn(FEATURE, "waypoint refused for player " + playerId + ": no body");
			return LF_EWelcomeWaypoint.NO_BODY;
		}

		LF_QuestTrackerComponent tracker = LF_QuestTrackerComponent.Cast(body.FindComponent(LF_QuestTrackerComponent));
		if (!tracker)
		{
			LF_Debug.Warn(FEATURE, "waypoint refused for player " + playerId + ": the body has no quest tracker");
			return LF_EWelcomeWaypoint.NO_BODY;
		}

		string questId = tracker.RefreshGuide();
		if (questId.IsEmpty())
		{
			LF_Debug.Info(FEATURE, "waypoint for player " + playerId + ": no arrival errand is left");
			return LF_EWelcomeWaypoint.NO_STEP;
		}

		LF_QuestManager manager = LF_QuestManager.GetInstance();
		if (manager && manager.GetDefinitions().Contains(questId))
			labelKey = LF_ArrivalGuide.LabelOf(manager.GetDefinitions().Get(questId));

		LF_Debug.Info(FEATURE, "waypoint for player " + playerId + " set to arrival step " + questId);
		return LF_EWelcomeWaypoint.SET;
	}

	//------------------------------------------------------------------------------------------------
	//! The toast body for a waypoint answer.
	static string WaypointToastKey(LF_EWelcomeWaypoint result)
	{
		if (result == LF_EWelcomeWaypoint.SET)
			return "#LF-WelcomeGuide_WaypointSet";

		if (result == LF_EWelcomeWaypoint.NO_STEP)
			return "#LF-WelcomeGuide_WaypointNoStep";

		return "#LF-WelcomeGuide_WaypointRefused";
	}

	//------------------------------------------------------------------------------------------------
	//! Client: may the guide open by itself now? Not over another sheet or menu, not while the body
	//! is still getting out of bed (the wake loiter), never on a dead or missing body, never while a
	//! conversation is up (the guide would take the confirm key from the clerk scene).
	static bool CanAutoOpen(bool hasBody, bool alive, bool loitering, bool menuOpen, bool sheetOpen, bool dialogueOpen)
	{
		return hasBody && alive && !loitering && !menuOpen && !sheetOpen && !dialogueOpen;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: may the guide key open the sheet? Not while a conversation is up: the scene keeps focus.
	static bool CanOpenByKey(bool dialogueOpen)
	{
		return !dialogueOpen;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_WelcomeGuide's statics (LF_StaticReset finds it by reflection).
class LF_WelcomeGuide_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_WelcomeGuide";
	}

	override void Apply()
	{
		LF_WelcomeGuide.ResetStatics();
	}
}
