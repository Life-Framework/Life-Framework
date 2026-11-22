//------------------------------------------------------------------------------------------------
//! A bus stop: a roadside sign that registers in LF_Places under kind BusStop (the map draws the same
//! kind), names its stop and says where an arriving passenger is set down. The core holds no map:
//! the label is the one the placed stop sets, else the nearest map location, else the place id.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Travel", description: "A bus stop: registers as an LF_Places place of kind BusStop, names the stop and holds its arrival spot.")]
class LF_BusStopComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_BusStopComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, desc: "Stable stop id for LF_Places and the timetable. Empty uses the entity name, then the entity id.")]
	protected string m_sStopId;

	[Attribute("", UIWidgets.EditBox, desc: "What the stop is called on the timetable (a localization key). Empty names it after the nearest map location.")]
	protected string m_sLabel;

	[Attribute("1.0 0 0.6", UIWidgets.Coords, desc: "Where an arriving passenger stands, in the sign's own frame (metres): beside the sign on its face side (a placed stop has its back to a wall). Keep it on the pavement, off the road; the arrival probe searches round it for clear ground the sign can see.")]
	protected vector m_vArrival;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		LF_Places.Register(LF_BusStop.PLACE_KIND, GetStopId(), owner);
	}

	//------------------------------------------------------------------------------------------------
	string GetStopId()
	{
		if (!m_sStopId.IsEmpty())
			return m_sStopId;

		IEntity owner = GetOwner();
		if (!owner)
			return "";

		if (!owner.GetName().IsEmpty())
			return owner.GetName();

		// Never the EntityID: it differs between server and client, and the client sends this id back.
		// The position in whole metres is the same on both.
		vector at = owner.GetOrigin();
		return string.Format("busstop_%1_%2", Math.Round(at[0]), Math.Round(at[2]));
	}

	//------------------------------------------------------------------------------------------------
	//! \return The stop's name: the placed label, else the nearest map location, else the stop id.
	string GetLabel()
	{
		if (!m_sLabel.IsEmpty())
			return m_sLabel;

		IEntity owner = GetOwner();
		if (owner)
		{
			string named = LF_NamedPlaces.Nearest(owner.GetOrigin());
			if (!named.IsEmpty())
				return named;
		}

		return GetStopId();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The wanted arrival spot in world space (before the ground probe).
	vector GetArrivalWanted()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return vector.Zero;

		return owner.CoordToParent(m_vArrival);
	}

	//------------------------------------------------------------------------------------------------
	//! The label of a placed stop entity, or fallback when it has no stop component.
	static string LabelOf(IEntity stop, string fallback)
	{
		if (!stop)
			return fallback;

		LF_BusStopComponent component = LF_BusStopComponent.Cast(stop.FindComponent(LF_BusStopComponent));
		if (!component)
			return fallback;

		return component.GetLabel();
	}

	//------------------------------------------------------------------------------------------------
	//! The stop component of an entity, or null.
	static LF_BusStopComponent Of(IEntity stop)
	{
		if (!stop)
			return null;

		return LF_BusStopComponent.Cast(stop.FindComponent(LF_BusStopComponent));
	}
}
