//------------------------------------------------------------------------------------------------
//! Player actions for a configured breach target. Offer checks call CanArm / CanRepair only.
//! Wanted and the police notify live in LF_Breach.Arm, so a refreshed offer cannot fire them.
//------------------------------------------------------------------------------------------------
class LF_BreachArmAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Crime"))
			return false;

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

		if (!target.CanArmForOffer())
		{
			SetCannotPerformReason("#EL-Breach_AlreadyOpen");
			return false;
		}

		if (!LF_Breach.HasCharge(user))
		{
			SetCannotPerformReason("#EL-Breach_NeedCharge");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Crime", "LF_BreachArmAction"))
			return;

		if (!Replication.IsServer())
			return;

		LF_BreachTargetComponent target = LF_Component<LF_BreachTargetComponent>.Find(pOwnerEntity);
		if (!target)
		{
			LF_Debug.Error("Breach", "arm action: owner has no LF_BreachTargetComponent");
			return;
		}

		string playerUid = LF_Utils.GetPlayerUid(pUserEntity);
		if (playerUid.IsEmpty())
		{
			LF_Debug.Error("Breach", "arm action refused: player has no account id");
			return;
		}

		LF_Breach.Arm(target.GetTargetId(), playerUid, pUserEntity);
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

		return LF_Component<LF_BreachTargetComponent>.Find(GetOwner());
	}
}

//------------------------------------------------------------------------------------------------
class LF_BreachRepairAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Crime"))
			return false;

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

		if (!target.CanRepairForOffer(user))
		{
			SetCannotPerformReason("#EL-Breach_RepairDenied");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Crime", "LF_BreachRepairAction"))
			return;

		if (!Replication.IsServer())
			return;

		LF_BreachTargetComponent target = LF_Component<LF_BreachTargetComponent>.Find(pOwnerEntity);
		if (!target)
		{
			LF_Debug.Error("Breach", "repair action: owner has no LF_BreachTargetComponent");
			return;
		}

		string playerUid = LF_Utils.GetPlayerUid(pUserEntity);
		if (playerUid.IsEmpty())
		{
			LF_Debug.Error("Breach", "repair action refused: player has no account id");
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

		return LF_Component<LF_BreachTargetComponent>.Find(GetOwner());
	}
}
