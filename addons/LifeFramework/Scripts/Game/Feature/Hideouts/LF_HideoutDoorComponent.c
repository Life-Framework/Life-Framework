//------------------------------------------------------------------------------------------------
//! Binds a placed door to a hideout id. Members of the owning organization unlock
//! it through the existing door action. This component does not add an action.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Hideouts", description: "Door of an organization hideout.")]
class LF_HideoutDoorComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_HideoutDoorComponent : ScriptComponent
{
	[Attribute("Roadmap_Hideout", UIWidgets.EditBox, desc: "Hideout id. The owning organization id is written onto the lock.")]
	protected string m_sHideoutId;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		LF_Hideout.BindDoor(GetHideoutId(), owner);
	}

	//------------------------------------------------------------------------------------------------
	string GetHideoutId()
	{
		if (!m_sHideoutId.IsEmpty())
			return m_sHideoutId;

		if (GetOwner())
			return GetOwner().GetName();

		return "";
	}
}
