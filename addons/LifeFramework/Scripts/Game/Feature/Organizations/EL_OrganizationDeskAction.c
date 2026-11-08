//------------------------------------------------------------------------------------------------
//! Opens the registry ledger (LF_OrgLedger). The page is local. Cash and membership changes
//! go through the character RPC, which the server checks.
//------------------------------------------------------------------------------------------------
class EL_OrganizationDeskAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return user && GetOwner() && vector.Distance(user.GetOrigin(), GetOwner().GetOrigin()) <= 3.0;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!user || !GetOwner())
		{
			SetCannotPerformReason("#EL-Org_Empty");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The menu is local UI: run PerformAction only on the acting client. Without this a listen
	//! host also runs it for every other player and opens the menu on the host screen. Server
	//! work goes through the menu RPCs, which the server validates.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pUserEntity)
		{
			EL_Debug.Error("Organizations", "desk action refused: no player");
			return;
		}

		if (!LF_OrgLedger.Open(pOwnerEntity))
			EL_Debug.Error("Organizations", "registry ledger failed to open");
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Org_Open";
		return true;
	}
}
