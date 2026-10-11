//------------------------------------------------------------------------------------------------
//! Player actions for a configured breach target. Offer checks call CanArm / CanRepair only.
//! Wanted and the police notify live in LF_Breach.Arm, so a refreshed offer cannot fire them.
//------------------------------------------------------------------------------------------------
class LF_BreachArmAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return TargetComponent() != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		LF_BreachTargetComponent target = TargetComponent();
		if (!target)
		{
			SetCannotPerformReason("#EL-Breach_NotConfigured");
			return false;
		}

		if (!LF_Breach.CanArm(target.GetTargetId()))
		{
			SetCannotPerformReason("#EL-Breach_AlreadyOpen");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_BreachTargetComponent target = EL_Component<LF_BreachTargetComponent>.Find(pOwnerEntity);
		if (!target)
		{
			EL_Debug.Error("Breach", "arm action: owner has no LF_BreachTargetComponent");
			return;
		}

		string playerUid = EL_Utils.GetPlayerUid(pUserEntity);
		if (playerUid.IsEmpty())
		{
			EL_Debug.Error("Breach", "arm action refused: player has no account id");
			return;
		}

		LF_Breach.Arm(target.GetTargetId(), playerUid);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Breach_Arm";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_BreachTargetComponent TargetComponent()
	{
		if (!GetOwner())
			return null;

		return EL_Component<LF_BreachTargetComponent>.Find(GetOwner());
	}
}

//------------------------------------------------------------------------------------------------
class LF_BreachRepairAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return TargetComponent() != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		LF_BreachTargetComponent target = TargetComponent();
		if (!target)
		{
			SetCannotPerformReason("#EL-Breach_NotConfigured");
			return false;
		}

		string playerUid = EL_Utils.GetPlayerUid(user);
		if (!LF_Breach.CanRepair(target.GetTargetId(), playerUid))
		{
			SetCannotPerformReason("#EL-Breach_RepairDenied");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_BreachTargetComponent target = EL_Component<LF_BreachTargetComponent>.Find(pOwnerEntity);
		if (!target)
		{
			EL_Debug.Error("Breach", "repair action: owner has no LF_BreachTargetComponent");
			return;
		}

		string playerUid = EL_Utils.GetPlayerUid(pUserEntity);
		if (playerUid.IsEmpty())
		{
			EL_Debug.Error("Breach", "repair action refused: player has no account id");
			return;
		}

		LF_Breach.Repair(target.GetTargetId(), playerUid);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Breach_Repair";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_BreachTargetComponent TargetComponent()
	{
		if (!GetOwner())
			return null;

		return EL_Component<LF_BreachTargetComponent>.Find(GetOwner());
	}
}
