//------------------------------------------------------------------------------------------------
//! Opens the organization desk. The menu is local. Cash and membership changes
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
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pUserEntity)
		{
			EL_Debug.Error("Organizations", "desk action refused: no player");
			return;
		}

		EL_OrganizationMenu menu = EL_OrganizationMenu.Open();
		if (!menu)
		{
			EL_Debug.Error("Organizations", "organization menu failed to open");
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Org_Open";
		return true;
	}
}
