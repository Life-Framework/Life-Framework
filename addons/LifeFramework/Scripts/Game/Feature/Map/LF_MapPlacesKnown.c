// LF_MapPlacesKnown.c: discovery on the map (#2242, epic #2223, card M14; docs/architecture/everon-map.md
// section 4). The M13 store (LF_MapKnown) says what a character knows; the M4 layer (LF_MapPlacesUI) draws
// what the rule gives. This file closes the loop:
//   * a place that is unknown stays off the paper, heard of is a faded pencil "?", visited is the full icon,
//     and the open map follows a change while it is open (a board read, a clerk's mark, a visit);
//   * the clerk marks the arrival square's places at the end of the arrival chain;
//   * the tourist map item (the pack's LF_TouristMap.et) marks every live DISCOVER place as heard of.
// The rules are pure functions so LF_Test_MapPlacesKnown runs them with fixtures; the node emulator
// (`map render --as new|known`) draws the same three states.

//------------------------------------------------------------------------------------------------
//! Which places a mark reaches and how the layer tells a change. Pure; the server entry points below
//! feed it the live places.
class LF_MapPlacesKnown
{
	static const string FEATURE = "MapKnown";
	//! The quest turn-in that ends the arrival chain; the clerk marks the square's places then (D-M3).
	static const string ARRIVAL_QUEST_ID = "arrival_done";
	//! "The arrival square's places": the DISCOVER places within this many metres of the clerk's turn-in.
	static const float ARRIVAL_RADIUS = 300; // lf-fixed: everon-map.md section 4, the square and its streets; no coordinate in the core
	//! The quad a heard-of place draws (UI/Imagesets/LF_MapIcons.imageset): a pencil question mark.
	static const string HEARD_QUAD = "pencil_question";

	//------------------------------------------------------------------------------------------------
	//! What a DISCOVER place draws for what the player knows of it: nothing, faded, or full.
	static LF_EMapPoiState StateOf(LF_EMapPoiKnown known)
	{
		return LF_MapPoiRules.Resolve(LF_EMapPoiVisibility.DISCOVER, "", true, known, null);
	}

	//------------------------------------------------------------------------------------------------
	//! The quads a single place tries, in order. A heard-of place draws the pencil "?" first (the kind's
	//! own icon would give its type away), the rest draws the kind's own icon as the layer always did.
	static void Candidates(LF_EMapPoiState state, string kind, string icon, string category, notnull array<string> into)
	{
		if (state == LF_EMapPoiState.FADED)
			into.Insert(HEARD_QUAD);

		LF_MapPlaceIcons.Candidates(kind, icon, category, into);
	}

	//------------------------------------------------------------------------------------------------
	//! A short key of what the layer draws: place ids and states in list order. A change of it means the
	//! layer has to rebuild.
	static string Signature(notnull array<ref LF_MapPoi> pois)
	{
		string signature;
		foreach (LF_MapPoi poi : pois)
		{
			if (!poi)
				continue;

			signature += poi.m_sId + ":" + LF_MapPoiRules.StateName(poi.m_eState) + ";";
		}

		return signature;
	}

	//------------------------------------------------------------------------------------------------
	//! The turn-in that ends the arrival chain.
	static bool IsArrivalDone(string questId)
	{
		return questId == ARRIVAL_QUEST_ID;
	}

	//------------------------------------------------------------------------------------------------
	//! Live DISCOVER places, optionally only those within `radius` metres of `origin`; radius 0 or less
	//! takes them all (the tourist map). Hidden, role and public kinds are not marked: public places show
	//! already, and the underworld never comes from a printed map.
	//! \return How many ids were added.
	static int PlacesToHear(notnull LF_MapPoiConfig config, notnull array<ref LF_MapPoiPlace> places, vector origin, float radius, notnull array<string> into)
	{
		int added = 0;
		foreach (LF_MapPoiPlace place : places)
		{
			if (!place || !place.m_bLive || place.m_sId.IsEmpty())
				continue;

			LF_MapPoiKind row = config.Find(place.m_sKind);
			if (!row || row.m_eVisibility != LF_EMapPoiVisibility.DISCOVER)
				continue;

			if (radius > 0 && vector.Distance(origin, place.m_vPos) > radius)
				continue;

			into.Insert(place.m_sId);
			added++;
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Server store only: the character has heard of each place. Never lowers visited.
	//! \return How many were new to the character.
	static int HearAll(string characterId, notnull array<string> placeIds)
	{
		int fresh = 0;
		foreach (string placeId : placeIds)
		{
			if (LF_MapKnown.HearCharacter(characterId, placeId) == LF_EMapKnownOutcome.HEARD)
				fresh++;
		}

		return fresh;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: marks the live DISCOVER places within `radius` of `origin` (0: all of them) as heard of for
	//! the player's character, then sends the player the new set in one message.
	//! \return How many places are new on the player's map.
	static int HearAround(int playerId, vector origin, float radius)
	{
		if (!Replication.IsServer())
		{
			LF_Debug.Error(FEATURE, "HearAround called on a client; only the server records knowledge");
			return 0;
		}

		string characterId = LF_Utils.GetPlayerUID(playerId);
		LF_MapPoiConfig config = LF_MapPoiConfig.Get();
		if (characterId.IsEmpty() || !config)
		{
			LF_Debug.Warn(FEATURE, string.Format("marks refused: player %1 has no character key or the POI table did not load", playerId));
			return 0;
		}

		array<ref LF_MapPoiPlace> places = {};
		LF_MapPois.GatherPlaces(config, places);
		array<string> ids = {};
		PlacesToHear(config, places, origin, radius, ids);
		int fresh = HearAll(characterId, ids);
		LF_Debug.Info(FEATURE, string.Format("%1 marked %2 place(s) heard of (%3 new), radius %4", characterId, ids.Count(), fresh, radius));
		if (fresh > 0)
		{
			SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			if (controller)
				controller.LF_SendMapKnownAll();
		}

		return fresh;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the clerk's arrival marks, called as a quest is turned in. Only the arrival chain's last
	//! step marks anything.
	static void OnTurnedIn(IEntity player, string questId)
	{
		if (!IsArrivalDone(questId) || !player || !Replication.IsServer())
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(player);
		if (playerId <= 0)
			return;

		int fresh = HearAround(playerId, player.GetOrigin(), ARRIVAL_RADIUS);
		if (fresh > 0)
			LF_Utils.NotifyEntity(player, "#LF-MapKnown_ArrivalMarked", "#LF-MapKnown_Title", 5.0, fresh.ToString());
	}
}

//------------------------------------------------------------------------------------------------
//! The tourist map's one action (the pack's LF_TouristMap.et carries it): study it and every live
//! DISCOVER place of the island is pencilled onto the player's own map as heard of. Server side; the
//! item stays, so a second reading marks nothing new.
class LF_TouristMapAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-TouristMap_Use";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !pUserEntity)
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
		if (playerId <= 0)
			return;

		int fresh = LF_MapPlacesKnown.HearAround(playerId, pUserEntity.GetOrigin(), 0);
		if (fresh > 0)
			LF_Utils.NotifyEntity(pUserEntity, "#LF-TouristMap_Marked", "#LF-TouristMap_Title", 5.0, fresh.ToString());
		else
			LF_Utils.NotifyEntity(pUserEntity, "#LF-TouristMap_Nothing", "#LF-TouristMap_Title", 4.0);
	}
}

//------------------------------------------------------------------------------------------------
//! The clerk's arrival marks ride on the quest turn-in, so the quest files stay untouched.
modded class LF_QuestTrackerComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void GrantReward(LF_QuestDefinition def)
	{
		super.GrantReward(def);
		if (def)
			LF_MapPlacesKnown.OnTurnedIn(GetOwner(), def.m_sQuestId);
	}
}

//------------------------------------------------------------------------------------------------
//! The open map follows a change in what the player knows: a board read, the clerk's marks, a visit or
//! the tourist map, with no need to close and reopen it. A heard-of place draws the pencil "?".
modded class LF_MapPlacesUI
{
	protected static const int KNOWN_POLL_MS = 1000; // lf-fixed: how soon an open map shows a newly known place; a few places and no allocation when nothing changed

	protected string m_sKnownSignature;

	//------------------------------------------------------------------------------------------------
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);
		m_sKnownSignature = LF_MapPlacesKnown.Signature(m_aPois);
		GetGame().GetCallqueue().Remove(PollKnown);
		GetGame().GetCallqueue().CallLater(PollKnown, KNOWN_POLL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMapClose(MapConfiguration config)
	{
		GetGame().GetCallqueue().Remove(PollKnown);
		super.OnMapClose(config);
	}

	//------------------------------------------------------------------------------------------------
	//! Looks at what this player's map would show now; rebuilds the layer only when it differs.
	protected void PollKnown()
	{
		if (!m_wLayer)
			return;

		LF_MapPoiAudience audience = new LF_MapPoiAudience();
		LF_MapKnown.FillAudience(audience);
		array<ref LF_MapPoi> fresh = {};
		LF_MapPois.CollectLive(audience, fresh);
		string signature = LF_MapPlacesKnown.Signature(fresh);
		if (signature == m_sKnownSignature)
			return;

		m_sKnownSignature = signature;
		m_aPois.Clear();
		foreach (LF_MapPoi poi : fresh)
		{
			m_aPois.Insert(poi);
		}

		LF_Debug.Info(FEATURE, string.Format("known places changed: %1 places for this player", m_aPois.Count()));
		Rebuild();
	}

	//------------------------------------------------------------------------------------------------
	//! A single heard-of place draws the faded pencil "?" instead of its kind's icon.
	override protected void Style(int i)
	{
		super.Style(i);
		LF_MapClusterGroup group = m_aGroups[i];
		if (!group.IsSingle() || group.m_eState != LF_EMapPoiState.FADED)
			return;

		LF_MapPoi poi = m_aPois[group.m_aMembers[0]];
		array<string> candidates = {};
		LF_MapPlacesKnown.Candidates(group.m_eState, poi.m_sKind, poi.m_sIcon, poi.m_sCategory, candidates);
		LF_MapPlaceIcons.Apply(ImageWidget.Cast(m_aWidgets[i].FindAnyWidget("Icon")), candidates);
	}
}
