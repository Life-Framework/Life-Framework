//------------------------------------------------------------------------------------------------
//! A taxi stand (#2691): a sign by the road that registers in LF_Places under kind `taxi`, so a fare
//! and the map can name "the stand at <place>". The core holds no map: the stand's label is the one
//! its prefab sets, else the nearest map location (LF_NamedPlaces), else its place id.
//------------------------------------------------------------------------------------------------
class LF_TaxiStand
{
	static const string FEATURE = "TaxiStand";

	//! LF_Places kind of a placed taxi stand.
	static const string PLACE_KIND = "taxi";
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Jobs", description: "A taxi stand: registers as an LF_Places place of kind taxi and names its place.")]
class LF_TaxiStandComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_TaxiStandComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, desc: "Stable place id for LF_Places. Empty uses the entity name, then the entity id.")]
	protected string m_sPlaceId;

	[Attribute("", UIWidgets.EditBox, desc: "What the stand is called on a fare and the map (a localization key). Empty names it after the nearest map location.")]
	protected string m_sLabel;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		LF_Places.Register(LF_TaxiStand.PLACE_KIND, GetPlaceId(), owner);
	}

	//------------------------------------------------------------------------------------------------
	string GetPlaceId()
	{
		if (!m_sPlaceId.IsEmpty())
			return m_sPlaceId;

		IEntity owner = GetOwner();
		if (!owner)
			return "";

		if (!owner.GetName().IsEmpty())
			return owner.GetName();

		return string.Format("taxi_%1", owner.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! \return The name a fare gives this stand: the prefab's label, else the nearest map location's
	//! name, else the place id. Never empty on a registered stand.
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

		return GetPlaceId();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The registered taxi stand closest to pos on the ground plane, or null when the world
	//! has none.
	static LF_TaxiStandComponent Nearest(vector pos)
	{
		array<IEntity> stands = {};
		LF_Places.FindAll(LF_TaxiStand.PLACE_KIND, stands);

		LF_TaxiStandComponent best;
		float bestDist = 0;
		foreach (IEntity stand : stands)
		{
			LF_TaxiStandComponent component = LF_TaxiStandComponent.Cast(stand.FindComponent(LF_TaxiStandComponent));
			if (!component)
				continue;

			float dist = vector.DistanceXZ(stand.GetOrigin(), pos);
			if (!best || dist < bestDist)
			{
				best = component;
				bestDist = dist;
			}
		}

		return best;
	}
}
