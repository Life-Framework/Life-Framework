//------------------------------------------------------------------------------------------------
//! "Take the rent" on a site strongbox (#2684). The engine runs PerformAction on the server for the
//! acting character; the server alone checks the lease, the rank and the bank (LF_SiteStrongbox),
//! so the client only shows the prompt. A refusal is a toast and a log line.
class LF_SiteStrongboxAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return user != null && LF_Systems.IsEnabled(LF_OrgRules.SYSTEM);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		// No holder check here: the lease and the bank are server state, so a client check could grey out
		// a take the server would allow. The server refuses with a toast.
		return user != null && GetOwner() != null;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses(LF_OrgRules.SYSTEM, "LF_SiteStrongboxAction"))
			return;

		if (!Replication.IsServer())
			return;

		if (!pUserEntity)
		{
			LF_Debug.Error(LF_SiteStrongbox.FEATURE, "strongbox action performed without a character");
			return;
		}

		int amount;
		LF_ESiteStrongboxResult result = LF_SiteStrongbox.TakeFromBox(pOwnerEntity, pUserEntity, amount);
		LF_Utils.NotifyEntity(pUserEntity, LF_SiteStrongbox.ResultKey(result), "#LF-SiteStrongbox_Title", 4.0, amount.ToString());
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-SiteStrongbox_Take";
		return true;
	}
}
