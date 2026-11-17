//------------------------------------------------------------------------------------------------
//! Marks the vanilla building around this entity as fitted, so it is indestructible (D15). `town fit`
//! puts it on the slot's place marker beside LF_PlaceComponent; the marker stands inside the building.
//! On the server, one frame after load (so the base map's building exists), LF_FittedBuildings finds
//! the building with one sphere query and turns its damage handling off. Nothing runs per frame.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Core", description: "Makes the vanilla building around this marker indestructible (fitted buildings, D15).")]
class LF_FittedBuildingComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_FittedBuildingComponent : ScriptComponent
{
	[Attribute("25", UIWidgets.Auto, desc: "How far from the marker the building may stand, in metres. The building whose bounds hold the marker wins.", params: "1 100 1")]
	protected float m_fSearchRadius;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		GetGame().GetCallqueue().CallLater(FitNow, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void FitNow()
	{
		IEntity owner = GetOwner();
		if (!owner || !Replication.IsServer())
			return;

		LF_FittedBuildings.Fit(owner.GetOrigin(), m_fSearchRadius, PlaceId(owner));
	}

	//------------------------------------------------------------------------------------------------
	protected string PlaceId(IEntity owner)
	{
		LF_PlaceComponent place = LF_PlaceComponent.Cast(owner.FindComponent(LF_PlaceComponent));
		if (place)
			return place.GetPlaceId();

		return owner.GetName();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(FitNow);

		super.OnDelete(owner);
	}
}
