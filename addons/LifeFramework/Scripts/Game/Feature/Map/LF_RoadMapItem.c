// LF_RoadMapItem.c: the printed road map (#4474, WO-W2.4). A folded paper map a newsagent sells; studying it
// pencils every live town onto the reader's own map as "heard of" through LF_MapKnown (the store the town
// board, the clerk and the rumor pool use). Jobs, coves, the underworld and public places are not marked:
// jobs and coves are for rumors and work boards to give, public places show already. In the world the map is
// an action on the item, not a window.

//------------------------------------------------------------------------------------------------
//! Which places the road map marks. Pure; the action below feeds it the live places.
class LF_RoadMapRules
{
	static const string FEATURE = "MapKnown";
	//! The POI category of a town row. A pack names its town kind with this category and the DISCOVER
	//! visibility; no kind or place name lives in the core.
	static const string TOWN_CATEGORY = "town";

	//------------------------------------------------------------------------------------------------
	//! The live town places: a kind row in the town category that shows once heard of (DISCOVER). A
	//! place that is not live, a hidden or role kind and every other category (work, coves, shops) are left out.
	//! \return How many ids were added to `into`.
	static int TownsToHear(notnull LF_MapPoiConfig config, notnull array<ref LF_MapPoiPlace> places, notnull array<string> into)
	{
		int added = 0;
		foreach (LF_MapPoiPlace place : places)
		{
			if (!place || !place.m_bLive || place.m_sId.IsEmpty())
				continue;

			LF_MapPoiKind row = config.Find(place.m_sKind);
			if (!row || row.m_eVisibility != LF_EMapPoiVisibility.DISCOVER || row.m_sCategory != TOWN_CATEGORY)
				continue;

			into.Insert(place.m_sId);
			added++;
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: marks every live town as heard of for the player's character, then sends the player the
	//! new set in one message.
	//! \return How many towns are new on the player's map.
	static int HearTowns(int playerId)
	{
		if (!Replication.IsServer())
		{
			LF_Debug.Error(FEATURE, "HearTowns called on a client; only the server records knowledge");
			return 0;
		}

		string characterId = LF_Utils.GetPlayerUID(playerId);
		LF_MapPoiConfig config = LF_MapPoiConfig.Get();
		if (characterId.IsEmpty() || !config)
		{
			LF_Debug.Warn(FEATURE, string.Format("road map refused: player %1 has no character key or the POI table did not load", playerId));
			return 0;
		}

		array<ref LF_MapPoiPlace> places = {};
		LF_MapPois.GatherPlaces(config, places);
		array<string> ids = {};
		TownsToHear(config, places, ids);
		int fresh = LF_MapPlacesKnown.HearAll(characterId, ids);
		LF_Debug.Info(FEATURE, string.Format("%1 studied the road map: %2 town(s), %3 new", characterId, ids.Count(), fresh));
		if (fresh > 0)
		{
			SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			if (controller)
				controller.LF_SendMapKnownAll();
		}

		return fresh;
	}
}

//------------------------------------------------------------------------------------------------
//! The road map's one action (the core's LF_RoadMap.et carries it). Server side; the item stays, so a
//! second reading marks nothing new.
class LF_RoadMapAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-RoadMap_Use";
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

		int fresh = LF_RoadMapRules.HearTowns(playerId);
		if (fresh > 0)
			LF_Utils.NotifyEntity(pUserEntity, "#LF-RoadMap_Marked", "#LF-RoadMap_Title", 5.0, fresh.ToString());
		else
			LF_Utils.NotifyEntity(pUserEntity, "#LF-RoadMap_Nothing", "#LF-RoadMap_Title", 4.0);
	}
}
