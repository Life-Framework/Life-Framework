//------------------------------------------------------------------------------------------------
//! One live place as the map sees it before the rule runs: a place of a kind plus its position.
//! Pure data, so the rule runs in LOGIC tests with no world.
class LF_MapPoiPlace
{
	string m_sKind;
	string m_sId;
	vector m_vPos;
	bool m_bLive;

	//------------------------------------------------------------------------------------------------
	static LF_MapPoiPlace Make(string kind, string id, vector pos, bool live)
	{
		LF_MapPoiPlace place = new LF_MapPoiPlace();
		place.m_sKind = kind;
		place.m_sId = id;
		place.m_vPos = pos;
		place.m_bLive = live;
		return place;
	}
}

//------------------------------------------------------------------------------------------------
//! What one player holds: the roles they are on duty in and what they know of each place. The
//! discovery card (M13) fills the known set from the saved `mapknown` rows; this holder is the seam.
class LF_MapPoiAudience
{
	protected ref array<string> m_aRoles = {};
	protected ref map<string, LF_EMapPoiKnown> m_mKnown = new map<string, LF_EMapPoiKnown>();

	//------------------------------------------------------------------------------------------------
	void AddRole(string role)
	{
		if (!role.IsEmpty() && m_aRoles.Find(role) == -1)
			m_aRoles.Insert(role);
	}

	//------------------------------------------------------------------------------------------------
	array<string> GetRoles()
	{
		return m_aRoles;
	}

	//------------------------------------------------------------------------------------------------
	//! Records what the player knows of a place. Visited never drops back to heard of.
	void SetKnown(string placeId, LF_EMapPoiKnown known)
	{
		if (placeId.IsEmpty() || known == LF_EMapPoiKnown.NONE)
			return;

		if (KnownOf(placeId) == LF_EMapPoiKnown.VISITED)
			return;

		m_mKnown.Set(placeId, known);
	}

	//------------------------------------------------------------------------------------------------
	LF_EMapPoiKnown KnownOf(string placeId)
	{
		LF_EMapPoiKnown known;
		if (m_mKnown.Find(placeId, known))
			return known;

		return LF_EMapPoiKnown.NONE;
	}
}

//------------------------------------------------------------------------------------------------
//! One POI the map draws now: a live place, its row, and the state the rule gave this player.
class LF_MapPoi
{
	string m_sKind;
	string m_sId;
	vector m_vPos;
	LF_EMapPoiState m_eState;
	string m_sCategory;
	string m_sIcon;
	string m_sLabel;
	float m_fMinZoom;
	//! The nearest named place (a town), for the label "Bank, Saint-Philippe". Empty in logic tests.
	string m_sTown;
}

//------------------------------------------------------------------------------------------------
//! Reads LF_Places, applies the POI table and the visibility rule, and lists what a player sees
//! (docs/architecture/everon-map.md section 1). Client side: LF_Places holds every placed entity on
//! every machine, so no network traffic. The drawing layer is card M4 (LF_MapPlacesUI).
class LF_MapPois
{
	//------------------------------------------------------------------------------------------------
	//! The visible list for one audience over a given set of places, in place order. Places of a kind
	//! with no row, or whose state is NONE, are left out. Pure: the logic tests call it with fixtures.
	//! \return How many POIs were added to into.
	static int Collect(notnull LF_MapPoiConfig config, notnull LF_MapPoiAudience audience, notnull array<ref LF_MapPoiPlace> places, notnull array<ref LF_MapPoi> into)
	{
		int added = 0;
		foreach (LF_MapPoiPlace place : places)
		{
			if (!place)
				continue;

			LF_MapPoiKind row = config.Find(place.m_sKind);
			if (!row)
				continue;

			LF_EMapPoiState state = LF_MapPoiRules.Resolve(row.m_eVisibility, row.m_sRole, place.m_bLive, audience.KnownOf(place.m_sId), audience.GetRoles());
			if (state == LF_EMapPoiState.HIDDEN)
				continue;

			LF_MapPoi poi = new LF_MapPoi();
			poi.m_sKind = place.m_sKind;
			poi.m_sId = place.m_sId;
			poi.m_vPos = place.m_vPos;
			poi.m_eState = state;
			poi.m_sCategory = row.m_sCategory;
			poi.m_sIcon = row.m_sIcon;
			poi.m_sLabel = row.m_sLabel;
			poi.m_fMinZoom = row.m_fMinZoom;
			into.Insert(poi);
			added++;
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Every live place whose kind has a POI row, read from LF_Places. A place LF_Places holds is
	//! live: a dark map state never registers (#2208).
	static void GatherPlaces(notnull LF_MapPoiConfig config, notnull array<ref LF_MapPoiPlace> into)
	{
		if (!config.m_aKinds)
			return;

		foreach (LF_MapPoiKind row : config.m_aKinds)
		{
			if (!row || row.m_sKind.IsEmpty())
				continue;

			array<string> ids = {};
			array<IEntity> entities = {};
			LF_Places.FindAllWithIds(row.m_sKind, ids, entities);
			foreach (int i, IEntity entity : entities)
			{
				vector transform[4];
				entity.GetWorldTransform(transform);
				into.Insert(LF_MapPoiPlace.Make(row.m_sKind, ids[i], transform[3], true));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! What this audience sees on the map now: the live places, the table and the rule, with each
	//! POI's town from the nearest named place. Null config (the table did not load) lists nothing.
	//! \return How many POIs were added to into.
	static int CollectLive(notnull LF_MapPoiAudience audience, notnull array<ref LF_MapPoi> into)
	{
		LF_MapPoiConfig config = LF_MapPoiConfig.Get();
		if (!config)
			return 0;

		array<ref LF_MapPoiPlace> places = {};
		GatherPlaces(config, places);
		int first = into.Count();
		int added = Collect(config, audience, places, into);
		for (int i = first; i < into.Count(); i++)
		{
			into[i].m_sTown = LF_NamedPlaces.Nearest(into[i].m_vPos);
		}

		return added;
	}
}

//------------------------------------------------------------------------------------------------
//! A lookup LF_Places lacks: the live places of a kind with their ids. LF_Places.FindAll returns
//! entities only and the map keys what a player knows by place id.
modded class LF_Places
{
	//------------------------------------------------------------------------------------------------
	//! Appends the id and entity of every live place of a kind to the two arrays, in step.
	//! \return How many were added.
	static int FindAllWithIds(string kind, notnull array<string> ids, notnull array<IEntity> entities)
	{
		map<string, EntityID> held = Kind(kind, false);
		if (!held)
			return 0;

		int added = 0;
		array<string> stale = {};
		foreach (string id, EntityID entityId : held)
		{
			IEntity entity = Resolve(entityId);
			if (!entity)
			{
				stale.Insert(id);
				continue;
			}

			ids.Insert(id);
			entities.Insert(entity);
			added++;
		}

		foreach (string id : stale)
			held.Remove(id);

		return added;
	}
}
