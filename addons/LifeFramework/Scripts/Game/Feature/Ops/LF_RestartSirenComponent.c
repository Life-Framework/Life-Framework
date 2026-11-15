//------------------------------------------------------------------------------------------------
//! The restart siren (#1345): a placed pole siren that registers with LF_Places under the kind
//! "restart-siren" and does nothing else yet. The placer puts the prefab where the siren stands;
//! the warning logic that sounds it (O7) reads the siren through LF_Places, never by entity name.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Ops", description: "Placed restart siren. Registers itself with LF_Places as 'restart-siren'.")]
class LF_RestartSirenComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_RestartSirenComponent : ScriptComponent
{
	static const string PLACE_KIND = "restart-siren";
	static const string DEFAULT_ID = "main";

	[Attribute(DEFAULT_ID, UIWidgets.EditBox, desc: "Id of this siren among the restart sirens. Unique per world; leave as is when the world has one.")]
	protected string m_sSirenId;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
			return;

		if (!owner)
		{
			LF_Debug.Error("Ops", "restart siren component has no owner");
			return;
		}

		string id = m_sSirenId;
		if (id.IsEmpty())
			id = DEFAULT_ID;

		LF_Places.Register(PLACE_KIND, id, owner);
	}
}
