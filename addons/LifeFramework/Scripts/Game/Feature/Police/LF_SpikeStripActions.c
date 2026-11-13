//------------------------------------------------------------------------------------------------
//! Deploy: on the folded strip (SpikeStrip_Item.et). Shown to on-duty police only; the server
//! decides again in LF_SpikeStrips.Deploy and lays the strip in front of the officer.
//------------------------------------------------------------------------------------------------
class LF_SpikeStripDeployAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!GetOwner())
			return false;

		return LF_SpikeStrips.IsOnDutyOfficer(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!LF_SpikeStrips.IsOnDutyOfficer(user))
		{
			SetCannotPerformReason("#LF-SpikeStrip_PoliceOnly");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_SpikeStrips.Deploy(pUserEntity, pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-SpikeStrip_Deploy";
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Pick up: on the deployed strip (SpikeStrip.et). On-duty police fold it back into their kit.
//------------------------------------------------------------------------------------------------
class LF_SpikeStripPickUpAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!GetOwner())
			return false;

		return LF_SpikeStrips.IsOnDutyOfficer(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!LF_SpikeStrips.IsOnDutyOfficer(user))
		{
			SetCannotPerformReason("#LF-SpikeStrip_PoliceOnly");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_SpikeStrips.PickUp(pUserEntity, pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-SpikeStrip_PickUp";
		return true;
	}
}
