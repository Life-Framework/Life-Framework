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
//! Collects the gossip residents that stand within range of a point.
class LF_GossipQuery
{
	bool m_bFound;
	ref array<IEntity> m_aGossips = {};

	//------------------------------------------------------------------------------------------------
	bool Collect(IEntity entity)
	{
		if (!entity)
			return true;

		if (LF_GossipBenchRules.IsGossipId(LF_ResidentSpawner.ResidentIdOf(entity)))
		{
			m_bFound = true;
			m_aGossips.Insert(entity);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The gossip who tells `def`: the one its row names as a giver, else the first one found.
	IEntity SpeakerFor(LF_RumorDef def)
	{
		if (def && def.m_aGivers)
		{
			foreach (IEntity gossip : m_aGossips)
			{
				if (gossip && def.m_aGivers.Find(LF_ResidentSpawner.ResidentIdOf(gossip)) != -1)
					return gossip;
			}
		}

		if (m_aGossips.IsEmpty())
			return null;

		return m_aGossips[0];
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
	//! The gossip residents within earshot of the bench.
	protected LF_GossipQuery Gossips()
	{
		LF_GossipQuery query = new LF_GossipQuery();
		GetGame().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(), LF_GossipBenchRules.RANGE_M, query.Collect, null, EQueryEntitiesFlags.DYNAMIC | EQueryEntitiesFlags.WITH_OBJECT);
		return query;
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
		LF_GossipQuery gossips = Gossips();
		bool gossipNear = gossips.m_bFound;
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
				Tell(playerId, def, gossips.SpeakerFor(def));
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
	//! Server: the gossip speaks the rumor's recorded line to the player, its caption shows, and the
	//! player's map gets the heard-of place (LF_RumorVoice.Deliver). A row with no recording still
	//! shows "You overhear the gossips" and marks the place.
	protected void Tell(int playerId, notnull LF_RumorDef def, IEntity speaker)
	{
		LF_RumorVoice.Deliver(playerId, speaker, def, "#LF-Gossip_Title", "#LF-Gossip_Overheard");
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: how many rumors this bench has told.
	int ToldCount()
	{
		return m_iToldCount;
	}
}
