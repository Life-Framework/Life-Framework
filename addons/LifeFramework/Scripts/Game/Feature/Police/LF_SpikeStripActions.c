//------------------------------------------------------------------------------------------------
//! Deploy: on the folded strip (SpikeStrip_Item.et). Shown to on-duty police only; the server
//! decides again in LF_SpikeStrips.Deploy and lays the strip in front of the officer. With the
//! preview flag on, selecting it shows the ghost (LF_SpikeStripPreview).
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
	//! Local player only: the ghost of where the strip will lie while Deploy is selected (#928
	//! prototype, behind LF_SpikeStripPreview.Enabled). The server decides again on Deploy.
	protected override void OnActionSelected()
	{
		super.OnActionSelected();
		if (!LF_SpikeStripPreview.Enabled() || !GetOwner())
			return;

		if (!LF_SpikeStrips.IsOnDutyOfficer(LF_Utils.GetLocalCharacter()))
			return;

		LF_SpikeStripPreview.BeginFor(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnActionDeselected()
	{
		super.OnActionDeselected();
		LF_SpikeStripPreview.End();
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
