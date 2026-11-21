//------------------------------------------------------------------------------------------------
//! The lock marker (#3203): drop LF_BuildingLock.et inside a vanilla building and nobody opens its
//! doors. Runs on every machine, one frame after load (so the base map's building exists); see
//! LF_BuildingLocks. The sign key is the door prompt's reason: Private, StaffOnly, ForSaleSoon,
//! OpeningSoon, Closed, or any pack key.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Houses", description: "Locks every door of the vanilla building around this marker, with a sign key as the reason.")]
class LF_BuildingLockComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_BuildingLockComponent : ScriptComponent
{
	[Attribute("#LF-BuildingLock_Private", UIWidgets.EditBox, desc: "Localization key shown on the door prompt: #LF-BuildingLock_Private, _StaffOnly, _ForSaleSoon, _OpeningSoon, _Closed, or a pack key.")]
	protected string m_sSignKey;

	[Attribute("#LF-BuildingLock_Private", UIWidgets.EditBox, desc: "House marker only (#3584): the key a stranger's prompt shows once the house has an owner.")]
	protected string m_sOwnedSignKey;

	[Attribute("25", UIWidgets.Auto, desc: "How far from the marker the building may stand, in metres. The building whose bounds hold the marker wins.", params: "1 100 1")]
	protected float m_fSearchRadius;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		GetGame().GetCallqueue().CallLater(RegisterNow, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Finds the building and locks it now (idempotent). Public so a test need not wait a frame.
	bool RegisterNow()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return false;

		return LF_BuildingLocks.Register(owner, m_fSearchRadius, m_sSignKey, m_sOwnedSignKey);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(RegisterNow);

		LF_BuildingLocks.Unregister(owner);
		super.OnDelete(owner);
	}
}
