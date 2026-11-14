//------------------------------------------------------------------------------------------------
//! Puts any entity in the LF_Places registry under a kind and id (#1045). For a place a system only
//! needs to find, with no behaviour of its own: a stop of the cash van's route, the vault's money
//! bag, where a debug dispatch call comes from. The system asks LF_Places by kind and id and never
//! by entity name, so a pack on another map places the component on whatever entity it likes.
//! A series that shares an id (the stops of one route) sets an order and registers as `id#order`.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Core", description: "Registers this entity as a place a system finds by kind and id.")]
class LF_PlaceComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_PlaceComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, desc: "What the place is, as the system that finds it names it: CashVan, CashVanStop, BankBag, DispatchSource.")]
	protected string m_sKind;

	[Attribute("", UIWidgets.EditBox, desc: "Which one: the route id of a van or its stops, the vault id of a money bag, a dispatch place id. Empty uses the entity name.")]
	protected string m_sId;

	[Attribute("-1", UIWidgets.Auto, desc: "Position in a series that shares the id (a route's stops, from 0). -1: not a series.")]
	protected int m_iOrder;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		string id = GetPlaceId();
		if (m_sKind.IsEmpty() || id.IsEmpty())
		{
			LF_Debug.Error(LF_Places.FEATURE, "place component has no kind or id and the entity is unnamed");
			return;
		}

		LF_Places.Register(m_sKind, LF_Places.OrderedId(id, m_iOrder), owner);
	}

	//------------------------------------------------------------------------------------------------
	string GetPlaceId()
	{
		if (!m_sId.IsEmpty())
			return m_sId;

		if (GetOwner())
			return GetOwner().GetName();

		return "";
	}
}
