//------------------------------------------------------------------------------------------------
//! "Fly the <name> flag" (#4449): one of these per sample flag on the registry flag wall; the prefab
//! sets which flag (m_iFlag, 1 to 20) each one flies. Shown only to a leader of an organization. The
//! pick runs on the server (LF_OrgFlagWall.PickBy), which checks the leader, the flag lock and the
//! flag number again and answers in words. No window opens.
//------------------------------------------------------------------------------------------------
class LF_OrgFlagWallAction : ScriptedUserAction
{
	[Attribute("1", UIWidgets.EditBox, "Which preset flag, 1 to 20, this action flies")]
	protected int m_iFlag;

	//------------------------------------------------------------------------------------------------
	int GetFlag()
	{
		return m_iFlag;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!user || !GetOwner() || !LF_Systems.IsEnabled(LF_OrgRules.SYSTEM))
			return false;

		return LF_OrgFlagWall.Offers(LF_OrgReplication.ViewOf(user));
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return user && GetOwner() && LF_OrgFlags.IsValid(m_iFlag);
	}

	//------------------------------------------------------------------------------------------------
	//! Runs on the server (the action is not local-only); a client's copy does nothing.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_OrgFlagWall.PickBy(pUserEntity, m_iFlag);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = WidgetManager.Translate("#LF-OrgFlagWall_Fly", WidgetManager.Translate(LF_OrgFlagWall.NameKey(m_iFlag)));
		return true;
	}
}
