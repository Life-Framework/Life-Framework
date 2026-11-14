//------------------------------------------------------------------------------------------------
//! Deploy: on the folded strip (SpikeStrip_Item.et). Shown to whoever the strip's LF_PlaceableComponent
//! allows (on-duty police). The officer's machine sends the spot its ghost shows as the action's
//! own data (OnSaveActionData); the server keeps it (OnLoadActionData) and PerformAction hands it to
//! LF_PlacementService.Confirm, which checks it again (#1026). No extra RPC. With no fresh proposal
//! (a server-side perform) the server places at its own plan.
//------------------------------------------------------------------------------------------------
class LF_SpikeStripDeployAction : ScriptedUserAction
{
	//! How long a proposal that arrived with the action's data is good for, until its perform.
	static const int PROPOSAL_MS = 3000; // lf-fixed: network slack between the data and the perform, not a gameplay knob

	protected bool m_bProposal;
	protected vector m_vProposalOrigin;
	protected vector m_vProposalForward;
	protected int m_iProposalTick;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Police"))
			return false;

		LF_PlaceableComponent placeable = LF_Component<LF_PlaceableComponent>.Find(GetOwner());
		return placeable && LF_PlacementService.MayPlace(user, placeable);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		LF_PlaceableComponent placeable = LF_Component<LF_PlaceableComponent>.Find(GetOwner());
		if (!placeable || !LF_PlacementService.MayPlace(user, placeable))
		{
			SetCannotPerformReason("#LF-SpikeStrip_PoliceOnly");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Police", "LF_SpikeStripDeployAction"))
			return;

		if (!Replication.IsServer())
			return;

		bool fresh = m_bProposal && System.GetTickCount() - m_iProposalTick <= PROPOSAL_MS;
		m_bProposal = false;
		if (!fresh)
		{
			LF_SpikeStrips.Deploy(pUserEntity, pOwnerEntity);
			return;
		}

		vector proposal[4];
		ProposalTransform(m_vProposalOrigin, m_vProposalForward, proposal);
		LF_SpikeStrips.DeployAt(pUserEntity, pOwnerEntity, proposal);
	}

	//------------------------------------------------------------------------------------------------
	//! The performing player's machine: the spot their ghost shows, else the plan from where they
	//! stand. On a listen host the data does not travel, so the host keeps it here as well.
	override protected bool OnSaveActionData(ScriptBitWriter writer)
	{
		IEntity item = GetOwner();
		vector origin;
		vector forward;
		string from = "ghost";
		if (!LF_SpikeStripPreview.CurrentSpot(item, origin, forward))
		{
			from = "plan";
			IEntity user = LF_Utils.GetLocalCharacter();
			LF_PlaceableComponent placeable = LF_Component<LF_PlaceableComponent>.Find(item);
			if (!user || !placeable)
				return false;

			vector transform[4];
			LF_PlacementProbe probe = new LF_PlacementProbe();
			LF_PlacementService.Plan(user, placeable, item, transform, probe);
			origin = transform[3];
			forward = transform[2];
		}

		writer.WriteVector(origin);
		writer.WriteVector(forward);
		LF_Debug.Info(LF_PlacementService.FEATURE, string.Format("proposal sent from the %1: %2 heading %3", from, origin, forward));
		if (Replication.IsServer())
			Keep(origin, forward);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The server: the proposal the performing player sent, kept until PerformAction.
	override protected bool OnLoadActionData(ScriptBitReader reader)
	{
		vector origin;
		vector forward;
		if (!reader.ReadVector(origin) || !reader.ReadVector(forward))
		{
			LF_Debug.Warn(LF_PlacementService.FEATURE, "proposal unreadable; the server will place at its own plan");
			return false;
		}

		if (!Replication.IsServer())
			return true;

		Keep(origin, forward);
		LF_Debug.Info(LF_PlacementService.FEATURE, string.Format("proposal received: %1 heading %2", origin, forward));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void Keep(vector origin, vector forward)
	{
		m_bProposal = true;
		m_vProposalOrigin = origin;
		m_vProposalForward = forward;
		m_iProposalTick = System.GetTickCount();
	}

	//------------------------------------------------------------------------------------------------
	//! A transform from a proposal's origin and forward (Confirm reads those two rows).
	static void ProposalTransform(vector origin, vector forward, out vector transform[4])
	{
		LF_PlacementGeometry.Transform(origin - forward * 0.5, origin + forward * 0.5, transform);
	}

	//------------------------------------------------------------------------------------------------
	//! Local player only: the ghost of where the strip will lie while Deploy is selected (#928
	//! prototype, behind LF_SpikeStripPreview.Enabled). The server decides again on Deploy.
	protected override void OnActionSelected()
	{
		super.OnActionSelected();
		if (!LF_SpikeStripPreview.Enabled() || !GetOwner())
			return;

		if (!CanBeShownScript(LF_Utils.GetLocalCharacter()))
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
		if (!LF_Systems.IsEnabled("Police"))
			return false;

		if (!GetOwner())
			return false;

		return LF_PoliceUtils.IsOnDutyCharacter(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!LF_PoliceUtils.IsOnDutyCharacter(user))
		{
			SetCannotPerformReason("#LF-SpikeStrip_PoliceOnly");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Police", "LF_SpikeStripPickUpAction"))
			return;

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
