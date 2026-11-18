//------------------------------------------------------------------------------------------------
//! Marks a place as one of the illicit buyer's spots (#2856). It registers its entity in LF_Places
//! under LF_BuyerRotation.PLACE_KIND on the server only; LF_BuyerRotation picks which spot is open.
//! A pack places the component where the buyer may stand; the core holds no spot of its own.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Laundering", description: "One spot the illicit buyer can stand at.")]
class LF_BuyerSpotComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_BuyerSpotComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, desc: "Stable spot id. Empty uses the entity name. The rotation sorts spots by this id.")]
	protected string m_sSpotId;

	//------------------------------------------------------------------------------------------------
	string GetSpotId()
	{
		if (!m_sSpotId.IsEmpty())
			return m_sSpotId;

		IEntity owner = GetOwner();
		if (owner)
			return owner.GetName();

		return "";
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		// A client never knows where the buyer may stand.
		if (!Replication.IsServer())
			return;

		string id = GetSpotId();
		if (id.IsEmpty())
		{
			LF_Debug.Error(LF_BuyerRotation.FEATURE, "buyer spot has no id and the entity is unnamed");
			return;
		}

		LF_Places.Register(LF_BuyerRotation.PLACE_KIND, id, owner);
	}
}
