//------------------------------------------------------------------------------------------------
//! Binds a placed door to a hideout id. Members of the owning organization unlock
//! it through the existing door action. This component does not add an action. It carries the
//! owner's name (the server writes it, clients read it) so the door can say whose it is (#934).
//------------------------------------------------------------------------------------------------
enum LF_EHideoutDoorView
{
	//! The viewer belongs to the owning organization.
	MEMBER,
	//! An organization holds the yard and the viewer is not in it.
	MEMBERS_ONLY,
	//! No organization holds the yard.
	UNOWNED
}

//------------------------------------------------------------------------------------------------
//! Pure wording rules of the hideout door: what it is to this viewer, what the prompt reads and
//! why it refuses. The door action applies them on client and server alike.
class LF_HideoutDoorRules
{
	//------------------------------------------------------------------------------------------------
	//! \param ownerOrgId The organization written on the door's lock; empty when nobody holds the yard.
	//! \param viewerOrgId The organization the viewer belongs to; empty when none.
	static LF_EHideoutDoorView View(string ownerOrgId, string viewerOrgId)
	{
		if (ownerOrgId.IsEmpty())
			return LF_EHideoutDoorView.UNOWNED;

		if (viewerOrgId == ownerOrgId)
			return LF_EHideoutDoorView.MEMBER;

		return LF_EHideoutDoorView.MEMBERS_ONLY;
	}

	//------------------------------------------------------------------------------------------------
	//! Key of the prompt on a locked door: "Open" for a member, "Members only (<org>)" for everyone else.
	static string LabelKey(LF_EHideoutDoorView view, string ownerName)
	{
		if (view == LF_EHideoutDoorView.MEMBER)
			return "#LF-Hideout_Open";

		if (view == LF_EHideoutDoorView.UNOWNED)
			return "#LF-Hideout_Unowned";

		if (ownerName.IsEmpty())
			return "#LF-Hideout_MembersOnlyPlain";

		return "#LF-Hideout_MembersOnly";
	}

	//------------------------------------------------------------------------------------------------
	//! Key of the refusal under the prompt; empty for a member.
	static string ReasonKey(LF_EHideoutDoorView view, string ownerName)
	{
		if (view == LF_EHideoutDoorView.MEMBER)
			return string.Empty;

		if (view == LF_EHideoutDoorView.UNOWNED)
			return "#LF-Hideout_UnownedReason";

		if (ownerName.IsEmpty())
			return "#EL-Hideout_NotMember";

		return "#LF-Hideout_MembersOnlyReason";
	}

	//------------------------------------------------------------------------------------------------
	//! The prompt, translated, with the organization's name filled in.
	static string Label(LF_EHideoutDoorView view, string ownerName)
	{
		return WidgetManager.Translate(LabelKey(view, ownerName), ownerName);
	}

	//------------------------------------------------------------------------------------------------
	//! The refusal, translated, with the organization's name filled in.
	static string Reason(LF_EHideoutDoorView view, string ownerName)
	{
		string key = ReasonKey(view, ownerName);
		if (key.IsEmpty())
			return key;

		return WidgetManager.Translate(key, ownerName);
	}
}

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

	//! Name of the owning organization: written by the server (LF_Hideout.PushLock), read by clients.
	[RplProp()]
	protected string m_sOwnerName;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		LF_Hideout.BindDoor(GetHideoutId(), owner);
	}

	//------------------------------------------------------------------------------------------------
	string GetOwnerName()
	{
		return m_sOwnerName;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only; a changed name is bumped once.
	void SetOwnerName(string name)
	{
		if (!Replication.IsServer() || m_sOwnerName == name)
			return;

		m_sOwnerName = name;
		Replication.BumpMe();
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
