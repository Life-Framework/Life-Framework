//------------------------------------------------------------------------------------------------
//! Marks an ItemProcessor as the hideout machine. LF_ProcessAction runs only when
//! the player's organization owns this hideout and has the processing upgrade.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Hideouts", description: "Processing machine of an organization hideout.")]
class LF_HideoutMachineComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_HideoutMachineComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, desc: "Hideout id (the layer sets it) this machine belongs to.")]
	protected string m_sHideoutId;

	//------------------------------------------------------------------------------------------------
	string GetHideoutId()
	{
		if (!m_sHideoutId.IsEmpty())
			return m_sHideoutId;

		return LF_Hideout.HIDEOUT_ID;
	}
}
