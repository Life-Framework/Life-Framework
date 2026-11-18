//------------------------------------------------------------------------------------------------
//! A fishing spot (#2929, chapter "Gone fishing", docs/architecture/fishing.md sections 2 and 5): a
//! marker a map maker drops on a quay, a cove or a lake shore. It says which table a cast inside its
//! radius rolls on and how many fish the spot holds before it thins out. It registers in LF_Places as
//! kind `FishingSpot` on every machine, so the cast finds it by kind (`FindSpotFor`), never by entity
//! name, layer or coordinate, and a pack on another map just places the prefab.
//!
//! The stock is server session state (LF_GatherStock): one fewer per catch, one back every regrow
//! interval, never saved; a restart refills every spot.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Fishing", description: "A fishing spot: the table, radius and stock of the water around this marker.")]
class LF_FishingSpotComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_FishingSpotComponent : ScriptComponent
{
	//! The LF_Places kind every fishing spot registers under.
	static const string KIND = "FishingSpot";

	[Attribute("", UIWidgets.EditBox, desc: "Which spot: a short id, unique in the world (quay, black_lake). Empty uses the entity name, then the entity id.")]
	protected string m_sId;

	[Attribute("harbour", UIWidgets.EditBox, desc: "Fish table a cast here rolls on (Configs/Fishing/LF_Fishing.conf: harbour, pond or a pack table). An unknown table falls back to open water at cast time.")]
	protected string m_sTable;

	[Attribute("12", UIWidgets.Auto, desc: "Radius in metres of the water this spot covers, measured flat from the marker", params: "1 500")]
	protected float m_fRadius;

	[Attribute("12", UIWidgets.Auto, desc: "Fish the spot holds before it thins out (an emptied spot gives open-water fish only)", params: "1 1000")]
	protected int m_iStock;

	[Attribute("120", UIWidgets.Auto, desc: "Seconds for one fish to grow back while the spot is below its stock. 0 never regrows.", params: "0 86400")]
	protected float m_fRegrowSeconds;

	[Attribute("-1", UIWidgets.Auto, desc: "Map state this spot belongs to (the pack says what each number means). It registers only while the mission header's m_aLF_LiveStates lists it. -1: always live.")]
	protected int m_iLiveState;

	protected ref LF_GatherStock m_Stock;
	protected bool m_bRegistered;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		string problem = Problem();
		if (!problem.IsEmpty())
		{
			LF_Debug.Error(LF_FishingConfig.FEATURE, string.Format("fishing spot '%1' not placed: %2", SpotId(owner), problem));
			return;
		}

		m_bRegistered = LF_PlaceLiveState.Register(KIND, SpotId(owner), m_iLiveState, LF_PlaceLiveState.HeaderStates(), owner);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_bRegistered)
			LF_Places.Unregister(KIND, SpotId(owner));

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Empty when the marker's data is usable, else why not.
	string Problem()
	{
		if (m_sTable.IsEmpty())
			return "no table";
		if (m_fRadius <= 0)
			return string.Format("radius %1 is not above 0", m_fRadius);
		if (m_iStock < 1)
			return string.Format("stock %1 is below 1", m_iStock);
		if (m_fRegrowSeconds < 0)
			return string.Format("regrow %1 is negative", m_fRegrowSeconds);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The id this spot registers under: its own, else the entity name, else the entity id (unique on
	//! one machine, which is all the registry needs: nothing here is saved or sent).
	protected string SpotId(IEntity owner)
	{
		if (!m_sId.IsEmpty())
			return m_sId;

		if (!owner.GetName().IsEmpty())
			return owner.GetName();

		return "spot_" + owner.GetID().ToString();
	}

	//------------------------------------------------------------------------------------------------
	string GetTable()
	{
		return m_sTable;
	}

	//------------------------------------------------------------------------------------------------
	float GetRadius()
	{
		return m_fRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when `position` lies on the water this spot covers (flat distance, so the height of
	//! the marker or the cast target does not matter).
	bool Covers(vector position)
	{
		return Distance(position) <= m_fRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Flat distance in metres from the marker to `position`.
	float Distance(vector position)
	{
		return Math.Sqrt(vector.DistanceSqXZ(GetOwner().GetOrigin(), position));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: fish left at `nowSeconds`, after regrowth. The stock starts full on first touch.
	int StockLeft(float nowSeconds)
	{
		return LF_FishingRules.StockLeft(Stock(nowSeconds), nowSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: takes one fish out of the spot. False (nothing taken) when it is emptied.
	bool TakeFish(float nowSeconds)
	{
		return LF_FishingRules.TakeFish(Stock(nowSeconds), nowSeconds);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_GatherStock Stock(float nowSeconds)
	{
		if (!m_Stock)
			m_Stock = LF_FishingRules.NewStock(m_iStock, m_fRegrowSeconds, nowSeconds);

		return m_Stock;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The component of a place entity, or null when it carries none.
	static LF_FishingSpotComponent Of(IEntity entity)
	{
		if (!entity)
			return null;

		return LF_FishingSpotComponent.Cast(entity.FindComponent(LF_FishingSpotComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Every live fishing spot, found by kind in LF_Places. \return How many were added.
	static int FindAll(notnull array<LF_FishingSpotComponent> into)
	{
		array<IEntity> places = {};
		LF_Places.FindAll(KIND, places);
		int added = 0;
		foreach (IEntity place : places)
		{
			LF_FishingSpotComponent spot = Of(place);
			if (!spot)
				continue;

			into.Insert(spot);
			added++;
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The spot a cast at `target` fishes: the registered spot whose radius covers it, the
	//! nearest marker when several do, and null when none does (the cast uses the open-water table).
	static LF_FishingSpotComponent FindSpotFor(vector target)
	{
		array<LF_FishingSpotComponent> spots = {};
		FindAll(spots);
		return PickCovering(spots, target);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Of `spots`, the one whose radius covers `target` and whose marker is nearest it; else null.
	static LF_FishingSpotComponent PickCovering(notnull array<LF_FishingSpotComponent> spots, vector target)
	{
		LF_FishingSpotComponent best;
		float bestDistance = 0;
		foreach (LF_FishingSpotComponent spot : spots)
		{
			if (!spot || !spot.Covers(target))
				continue;

			float distance = spot.Distance(target);
			if (!best || distance < bestDistance)
			{
				best = spot;
				bestDistance = distance;
			}
		}

		return best;
	}
}
