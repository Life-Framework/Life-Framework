// LF_GossipBenchComponent.c - the world side of "Sit and listen" (#4462, plan W5.3).
// Sits on a bench prefab that carries the base Sit actions. Server only: once a second it finds who
// sits on this bench (the base loiter), whether a gossip resident is within earshot, and steps the
// pure LF_GossipBenchState; a listen that completes tells the character one rumor through
// LF_GossipBench.Overhear and sends the place to their map. Nothing is persisted: the heard-of
// place is the persisted fact (LF_MapKnown).

[ComponentEditorProps(category: "LifeFramework/Residents", description: "Gossip bench: a player who sits here while a gossip is near overhears one rumor (#4462)")]
class LF_GossipBenchComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Collects whether a gossip resident stands within range of a point.
class LF_GossipQuery
{
	bool m_bFound;

	//------------------------------------------------------------------------------------------------
	bool Collect(IEntity entity)
	{
		if (m_bFound || !entity)
			return !m_bFound;

		if (LF_GossipBenchRules.IsGossipId(LF_ResidentSpawner.ResidentIdOf(entity)))
			m_bFound = true;

		return !m_bFound;
	}
}

//------------------------------------------------------------------------------------------------
class LF_GossipBenchComponent : ScriptComponent
{
	protected static const int TICK_MS = 1000;	// lf-fixed: the listen is counted in whole seconds

	protected ref LF_GossipBenchState m_State = new LF_GossipBenchState();
	protected ref LF_RumorState m_RumorState = new LF_RumorState();
	protected int m_iDays;
	protected int m_iLastMinute = -1;
	protected int m_iToldCount;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!Replication.IsServer() || !GetGame().InPlayMode())
			return;

		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! A running game minute: the day count grows each time the clock passes midnight.
	protected int Stamp()
	{
		int minute = LF_ResidentTimetable.NowMinute();
		if (m_iLastMinute >= 0 && minute < m_iLastMinute)
			m_iDays++;

		m_iLastMinute = minute;
		return m_iDays * LF_GossipBenchRules.MINUTES_PER_DAY + minute;
	}

	//------------------------------------------------------------------------------------------------
	//! True when a live gossip resident stands within earshot of the bench.
	bool GossipNear()
	{
		LF_GossipQuery query = new LF_GossipQuery();
		GetGame().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(), LF_GossipBenchRules.RANGE_M, query.Collect, null, EQueryEntitiesFlags.DYNAMIC | EQueryEntitiesFlags.WITH_OBJECT);
		return query.m_bFound;
	}

	//------------------------------------------------------------------------------------------------
	//! True when this body sits on this bench.
	protected bool IsSitting(IEntity body)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(body);
		if (!character)
			return false;

		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.GetCharacterController());
		return controller && controller.IsLoiteringOnEntity(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return;

		int stamp = Stamp();
		bool gossipNear = GossipNear();
		array<int> ids = {};
		players.GetPlayers(ids);
		array<string> seen = {};
		foreach (int playerId : ids)
		{
			IEntity body = players.GetPlayerControlledEntity(playerId);
			string characterId = LF_Utils.GetPlayerUID(body);
			if (characterId.IsEmpty())
				continue;

			seen.Insert(characterId);
			float seconds = TICK_MS / 1000.0;
			LF_RumorDef def = Step(characterId, IsSitting(body), gossipNear, seconds, stamp);
			if (def)
				SendToMap(playerId, def);
		}

		m_State.KeepOnly(seen);
	}

	//------------------------------------------------------------------------------------------------
	//! One step for one character; on a completed listen it tells one rumor. The seam the tests drive.
	//! \return The rumor the character overheard, or null.
	LF_RumorDef Step(string characterId, bool sitting, bool gossipNear, float dt, int stamp)
	{
		if (!m_State.Step(characterId, sitting, gossipNear, dt, stamp))
			return null;

		LF_RumorDef def = LF_GossipBench.Overhear(characterId, m_RumorState, stamp + m_iToldCount);
		if (def)
		{
			m_State.MarkHeard(stamp);
			m_iToldCount++;
		}

		return def;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player's map gets the heard-of place and a short note says so. The note is plain
	//! text, not a spoken line: the gossips' own rumor lines and their voices are the rumor rows' (W5.4).
	protected void SendToMap(int playerId, notnull LF_RumorDef def)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (controller)
			controller.LF_SendMapKnown(def.m_sPlaceId, LF_MapKnownRules.SavedState(LF_EMapPoiKnown.HEARD));

		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Gossip_Title", "#LF-Gossip_Overheard", 6.0, LF_ENotificationType.INFO);
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: how many rumors this bench has told.
	int ToldCount()
	{
		return m_iToldCount;
	}
}
