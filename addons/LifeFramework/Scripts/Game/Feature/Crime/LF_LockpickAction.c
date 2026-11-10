//------------------------------------------------------------------------------------------------
//! Marks a locked LF_LockComponent door as pickable or not (#411). A door without this component
//! offers no lockpick action; one with m_bPickable off (a vault, the jail door) offers it but the
//! server refuses with a logged reason, so a player learns the lock is not for a pick.
[ComponentEditorProps(category: "LifeFramework/Feature/Crime", description: "Lets a lockpick work this door's lock.")]
class LF_LockpickTargetComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_LockpickTargetComponent : ScriptComponent
{
	[Attribute("1", UIWidgets.CheckBox, desc: "Off for a lock a pick cannot work: a bank vault, the jail door.")]
	protected bool m_bPickable;

	//------------------------------------------------------------------------------------------------
	bool IsPickable()
	{
		return m_bPickable;
	}
}

//------------------------------------------------------------------------------------------------
//! Held action at a locked door: asks the server to start a pick attempt. The gate here is the UI
//! prompt; LF_Lockpick.Begin re-checks everything on the server.
class LF_LockpickAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		LF_LockComponent doorLock = LF_Component<LF_LockComponent>.Find(GetOwner());
		return doorLock && doorLock.IsLocked() && LF_Component<LF_LockpickTargetComponent>.Find(GetOwner()) != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		LF_ELockpickCheck check = LF_Lockpick.CheckDoor(GetOwner(), user);
		if (check == LF_ELockpickCheck.NONE)
			return true;

		SetCannotPerformReason(LF_LockpickRules.RefusalKey(check));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_Lockpick.Begin(pOwnerEntity, pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Lockpick_Action";
		return true;
	}
}
