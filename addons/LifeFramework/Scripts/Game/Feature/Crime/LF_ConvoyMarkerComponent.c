//------------------------------------------------------------------------------------------------
//! Placed convoy start or drop. The id is the world entity name.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Crime", description: "Convoy start or drop marker.")]
class LF_ConvoyMarkerComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_ConvoyMarkerComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, desc: "Marker id. Empty uses the entity name.")]
	protected string m_sMarkerId;

	[Attribute("0", UIWidgets.CheckBox, desc: "True on the drop marker.")]
	protected bool m_bDrop;

	//------------------------------------------------------------------------------------------------
	string GetMarkerId()
	{
		if (!m_sMarkerId.IsEmpty())
			return m_sMarkerId;

		if (GetOwner())
			return GetOwner().GetName();

		return "";
	}

	//------------------------------------------------------------------------------------------------
	bool IsDrop()
	{
		return m_bDrop;
	}
}
