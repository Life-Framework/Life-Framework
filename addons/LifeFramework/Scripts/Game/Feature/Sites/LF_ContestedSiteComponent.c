//------------------------------------------------------------------------------------------------
//! LF_ContestedSiteComponent.c: marks the centre of a contested site (#2680, gang war #1022). The
//! site guards one hideout; the component registers it with LF_Places under kind `contested-site`
//! and the hideout id, and starts the server sweep (LF_ContestedSites). Radius and rings come from
//! LF_SitesConfig.For(hideoutId); no coordinates live in script.
[ComponentEditorProps(category: "LifeFramework/Feature/Sites", description: "Centre of a contested site that guards one hideout.")]
class LF_ContestedSiteComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_ContestedSiteComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, desc: "Hideout id this site guards (the id of its LF_HideoutDoorComponent and of its sites.rules entry).")]
	protected string m_sHideoutId;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (m_sHideoutId.IsEmpty())
		{
			LF_Debug.Error("Sites", "contested site without a hideout id: not registered");
			return;
		}

		if (!LF_Places.Register(LF_ContestedSites.PLACE_KIND, m_sHideoutId, owner))
			return;

		LF_ContestedSites.Start();
	}

	//------------------------------------------------------------------------------------------------
	string GetHideoutId()
	{
		return m_sHideoutId;
	}
}
