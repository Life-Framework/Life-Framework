//------------------------------------------------------------------------------------------------
//! Opens the war sheet (LF_OrgWarSheet, #2675) at the registry desk: the second action beside the
//! ledger. The sheet is local UI; every declare and accept goes through the character RPCs the server
//! checks (LF_OrgWarCharacter.c).
//------------------------------------------------------------------------------------------------
class LF_OrgWarDeskAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return user && GetOwner() && LF_Systems.IsEnabled(LF_OrgRules.SYSTEM) && vector.Distance(user.GetOrigin(), GetOwner().GetOrigin()) <= 3.0;
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
	//! The sheet is local UI: run PerformAction only on the acting client (as the ledger action does).
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pUserEntity)
		{
			LF_Debug.Error(LF_OrgWarSheet.FEATURE, "war desk action refused: no player");
			return;
		}

		if (!LF_OrgWarSheet.Open(pOwnerEntity))
			LF_Debug.Error(LF_OrgWarSheet.FEATURE, "war sheet failed to open");
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-OrgWarSheet_Open";
		return true;
	}
}
