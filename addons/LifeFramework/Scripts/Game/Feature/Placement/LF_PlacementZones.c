//------------------------------------------------------------------------------------------------
//! Where each placeable kind may go (#2447, epic #927, refs #1030). The policy table
//! (Configs/Placement/LF_PlacementPolicy.conf) gives a kind a zone: anywhere, roads only, inside a
//! property the player owns or holds a key to, or within an organization base's radius. LF_PlacementZones
//! judges the proposed spot; LF_PlacementPolicy.ZoneAt runs it after the who rule, and the server's
//! Confirm (single and composition) asks ZoneAt. A house owner places a chair in their own house and not
//! in the neighbour's, because the house is the one nearest the spot, not the one nearest the player.
//! Roads come from the AI road network, houses from LF_HouseManagerComponent.Live, and a base from
//! LF_Places (kind "org_base", id = the organization id), so core holds no coordinate or name.
//------------------------------------------------------------------------------------------------

//! Where a kind may be placed.
enum LF_EPlacementWhere
{
	ANYWHERE,
	//! On or beside a road of the AI road network (within LF_PlacementZones.ROAD_HALF_WIDTH_M of its line).
	ROADS_ONLY,
	//! Inside a house the player owns or holds a key to (the house nearest the spot, within HOUSE_RANGE).
	OWNED_PROPERTY,
	//! Within the entry's radius of the player's organization's base.
	ORG_BASE
}

//------------------------------------------------------------------------------------------------
//! What the zone rules read about a spot and the player. `Judge` is pure over this; `Gather` fills it.
class LF_PlacementWhereFacts
{
	bool m_bOnRoad;
	//! The house nearest the spot within HOUSE_RANGE, empty when none.
	string m_sHouseId;
	bool m_bHouseOwner;
	bool m_bKeyHolder;
	//! The player's organization, empty when they are in none.
	string m_sOrgId;
	//! Whether the organization has a base placed (LF_Places "org_base"), and how far the spot lies from it.
	bool m_bBaseKnown;
	float m_fBaseDistance;
	float m_fBaseRadius;
}

//------------------------------------------------------------------------------------------------
class LF_PlacementZones
{
	//! The LF_Places kind a base registers under; the id is the organization's id.
	static const string ORG_BASE_KIND = "org_base";

	//! How far from a road's line a spot still counts as on the road (half a lane pair plus the verge).
	static const float ROAD_HALF_WIDTH_M = 6; // lf-fixed: geometry, how wide a road is for "roads only"

	//! How far from a built cottage's shell origin a spot is inside it (#3173). Not the prefab bounds:
	//! one fixed radius keeps the check a sphere query and covers every shell tier shipped.
	static const float PLOT_RADIUS_M = 8; // lf-fixed: geometry, how far from a build-site shell's origin counts as inside it

	static const string TOAST_ROAD = "#LF-Placement_NeedRoad";
	static const string TOAST_PROPERTY = "#LF-Placement_NeedProperty";
	static const string TOAST_BASE = "#LF-Placement_NeedBase";

	//------------------------------------------------------------------------------------------------
	//! Pure. Whether the facts satisfy `zone`. Returns empty when the spot is allowed, else the refusal
	//! tail naming the zone; `toastKey` is that zone's toast.
	static string Judge(LF_EPlacementWhere zone, notnull LF_PlacementWhereFacts facts, out string toastKey)
	{
		toastKey = "";
		if (zone == LF_EPlacementWhere.ROADS_ONLY)
		{
			if (facts.m_bOnRoad)
				return "";

			toastKey = TOAST_ROAD;
			return string.Format("is not on a road (zone ROADS_ONLY, within %1 m of a road line)", ROAD_HALF_WIDTH_M);
		}

		if (zone == LF_EPlacementWhere.OWNED_PROPERTY)
		{
			if (facts.m_sHouseId.IsEmpty())
			{
				toastKey = TOAST_PROPERTY;
				return string.Format("is not inside a property (no house within %1 m of the spot, zone OWNED_PROPERTY)", LF_PlacementPolicy.HOUSE_RANGE);
			}

			if (facts.m_bHouseOwner || facts.m_bKeyHolder)
				return "";

			toastKey = TOAST_PROPERTY;
			return string.Format("is in house %1, which they neither own nor hold a key to (zone OWNED_PROPERTY)", facts.m_sHouseId);
		}

		if (zone == LF_EPlacementWhere.ORG_BASE)
		{
			if (facts.m_sOrgId.IsEmpty())
			{
				toastKey = TOAST_BASE;
				return "is not in an organization (zone ORG_BASE)";
			}

			if (!facts.m_bBaseKnown)
			{
				toastKey = TOAST_BASE;
				return string.Format("has no base: organization %1 has none placed (zone ORG_BASE)", facts.m_sOrgId);
			}

			if (facts.m_fBaseDistance <= facts.m_fBaseRadius)
				return "";

			toastKey = TOAST_BASE;
			return string.Format("is %1 m from the base of organization %2 (zone ORG_BASE, radius %3 m)", facts.m_fBaseDistance, facts.m_sOrgId, facts.m_fBaseRadius);
		}

		// ANYWHERE, and any value this build does not know: the shipped default is open.
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Pure. The house facts for a spot: the house is the one nearest `at` among `houses` (id and
	//! origin, parallel arrays) within HOUSE_RANGE, then FillHouse says whether the character owns it or
	//! holds its key.
	static void FillHouseNearest(vector at, notnull array<string> houseIds, notnull array<vector> houseOrigins, notnull array<string> ownerKeys, string characterKey, notnull array<string> heldKeyHouseIds, notnull LF_PlacementWhereFacts facts)
	{
		int nearest = -1;
		float nearestDistance = LF_PlacementPolicy.HOUSE_RANGE;
		for (int i = 0; i < houseIds.Count(); i++)
		{
			float distance = vector.Distance(at, houseOrigins[i]);
			if (distance <= nearestDistance)
			{
				nearest = i;
				nearestDistance = distance;
			}
		}

		if (nearest < 0)
			return;

		LF_PlacementWhoFacts house = new LF_PlacementWhoFacts();
		LF_PlacementPolicy.FillHouse(houseIds[nearest], ownerKeys[nearest], characterKey, heldKeyHouseIds, house);
		facts.m_sHouseId = house.m_sHouseId;
		facts.m_bHouseOwner = house.m_bHouseOwner;
		facts.m_bKeyHolder = house.m_bKeyHolder;
	}

	//------------------------------------------------------------------------------------------------
	//! Server and client: the facts `zone` reads for `player` placing at `spot`. Only what the zone needs
	//! is gathered.
	//! \param baseRadius The entry's base radius (ORG_BASE).
	static LF_PlacementWhereFacts Gather(IEntity player, vector spot, LF_EPlacementWhere zone, float baseRadius)
	{
		LF_PlacementWhereFacts facts = new LF_PlacementWhereFacts();
		if (zone == LF_EPlacementWhere.ROADS_ONLY)
			facts.m_bOnRoad = IsOnRoad(spot);
		else if (zone == LF_EPlacementWhere.OWNED_PROPERTY)
			GatherHouse(player, spot, facts);
		else if (zone == LF_EPlacementWhere.ORG_BASE)
			GatherBase(player, spot, baseRadius, facts);

		return facts;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the AI road network has a road line within ROAD_HALF_WIDTH_M of `at`. A world with no AI
	//! road network (DebugWorld) has none, so a roads-only kind is refused there.
	static bool IsOnRoad(vector at)
	{
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld || !aiWorld.GetRoadNetworkManager())
			return false;

		BaseRoad road;
		float roadDistance;
		aiWorld.GetRoadNetworkManager().GetClosestRoad(at, road, roadDistance);
		if (!road)
			return false;

		array<vector> points = {};
		road.GetPoints(points);
		float segmentDistance;
		vector direction = LF_PlacementGeometry.NearestSegmentDirection(points, at, segmentDistance);
		return direction != vector.Zero && segmentDistance <= ROAD_HALF_WIDTH_M;
	}

	//------------------------------------------------------------------------------------------------
	protected static void GatherHouse(IEntity player, vector spot, notnull LF_PlacementWhereFacts facts)
	{
		array<string> heldKeys = {};
		string characterKey;
		if (player)
		{
			LF_PlacementPolicy.HeldKeyHouseIds(player, heldKeys);
			characterKey = LF_Houses.CharacterKeyOf(player);
		}

		GatherHouseFor(spot, characterKey, heldKeys, facts);
	}

	//------------------------------------------------------------------------------------------------
	//! The house facts for a spot from the live houses, for a character key and the house ids of the keys
	//! they carry (the tests pass these without a player).
	static void GatherHouseFor(vector spot, string characterKey, notnull array<string> heldKeys, notnull LF_PlacementWhereFacts facts)
	{
		array<string> ids = {};
		array<vector> origins = {};
		array<string> ownerKeys = {};
		map<string, LF_HouseManagerComponent> live = LF_HouseManagerComponent.Live();
		foreach (string id, LF_HouseManagerComponent house : live)
		{
			if (!house || !house.GetOwner())
				continue;

			ids.Insert(house.GetHouseIdentifier());
			origins.Insert(house.GetOwner().GetOrigin());
			ownerKeys.Insert(house.GetOwnerKey());
		}

		// A finished build-site cottage is a property too (#3173): a spot inside its shell belongs to the plot.
		LF_PlacementWhoFacts plot = new LF_PlacementWhoFacts();
		if (GatherPlot(spot, characterKey, heldKeys, plot))
		{
			facts.m_sHouseId = plot.m_sHouseId;
			facts.m_bHouseOwner = plot.m_bHouseOwner;
			facts.m_bKeyHolder = plot.m_bKeyHolder;
			return;
		}

		FillHouseNearest(spot, ids, origins, ownerKeys, characterKey, heldKeys, facts);
	}

	//------------------------------------------------------------------------------------------------
	//! The finished build-site plot whose shell stands within PLOT_RADIUS_M of `at` (flat distance to the
	//! shell origin), filled into house facts keyed by the plot key. An unfinished plot is no property.
	//! The server reads the owner from LF_Properties on every call, so a plot sold mid-session stops
	//! accepting its old owner at once. A client has no owner key (only the replicated "owned" flag), so
	//! it lets the prompt show over an owned cottage and the server's Confirm decides.
	//! \return True when a finished plot covers `at` and the facts name it.
	static bool GatherPlot(vector at, string characterKey, notnull array<string> heldKeys, notnull LF_PlacementWhoFacts facts)
	{
		LF_BuildSiteComponent site = FinishedPlotAt(at);
		if (!site)
			return false;

		string plotKey = site.GetSiteKey();
		if (Replication.IsServer())
		{
			LF_PlacementPolicy.FillHouse(plotKey, LF_Properties.GetOwnerKey(plotKey), characterKey, heldKeys, facts);
			return true;
		}

		LF_PlacementPolicy.FillHouse(plotKey, "", characterKey, heldKeys, facts);
		if (site.IsOwned())
			facts.m_bHouseOwner = true;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The build site of a finished plot whose shell origin lies within PLOT_RADIUS_M of `at`; the nearest
	//! when several do, null when none.
	static LF_BuildSiteComponent FinishedPlotAt(vector at)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return null;

		LF_PlacementPlotQuery query = new LF_PlacementPlotQuery(at, PLOT_RADIUS_M);
		world.QueryEntitiesBySphere(at, LF_PlacementPolicy.HOUSE_RANGE, query.Collect, null, EQueryEntitiesFlags.ALL);
		return query.m_Nearest;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether every stage of the site's plot is built. The server reads the record; a client the
	//! replicated mirror.
	static bool IsPlotFinished(notnull LF_BuildSiteComponent site)
	{
		if (!Replication.IsServer())
			return site.IsComplete();

		string tier = site.GetTierId();
		int stages = LF_PropertyConfig.StageCount(tier);
		LF_PropertyRecord record = LF_Properties.Get(site.GetSiteKey());
		if (!record || stages <= 0)
			return false;

		return record.m_iBuildStage >= stages;
	}

	//------------------------------------------------------------------------------------------------
	protected static void GatherBase(IEntity player, vector spot, float radius, notnull LF_PlacementWhereFacts facts)
	{
		facts.m_fBaseRadius = radius;
		if (!player)
			return;

		string characterId = LF_OrganizationPlayer.CharacterId(player);
		if (characterId.IsEmpty())
			return;

		LF_OrganizationManager manager = LF_OrganizationManager.GetInstance();
		if (!manager)
			return;

		LF_Organization org = manager.GetByCharacter(characterId);
		if (!org)
			return;

		facts.m_sOrgId = org.GetId();
		vector base;
		if (!LF_Places.FindOrigin(ORG_BASE_KIND, facts.m_sOrgId, base))
			return;

		facts.m_bBaseKnown = true;
		facts.m_fBaseDistance = vector.Distance(Vector(spot[0], 0, spot[2]), Vector(base[0], 0, base[2]));
	}
}

//------------------------------------------------------------------------------------------------
//! Sphere-query collector: the finished build site whose shell origin lies nearest the spot, within range.
class LF_PlacementPlotQuery
{
	vector m_vAt;
	float m_fNearest;
	LF_BuildSiteComponent m_Nearest;

	//------------------------------------------------------------------------------------------------
	void LF_PlacementPlotQuery(vector at, float range)
	{
		m_vAt = at;
		m_fNearest = range;
	}

	//------------------------------------------------------------------------------------------------
	bool Collect(IEntity entity)
	{
		LF_BuildSiteComponent site = LF_Component<LF_BuildSiteComponent>.Find(entity);
		if (!site || site.GetSiteKey().IsEmpty() || !LF_PlacementZones.IsPlotFinished(site))
			return true;

		vector shell = site.GetShellOrigin();
		float distance = vector.Distance(Vector(m_vAt[0], 0, m_vAt[2]), Vector(shell[0], 0, shell[2]));
		if (distance <= m_fNearest)
		{
			m_Nearest = site;
			m_fNearest = distance;
		}

		return true;
	}
}
