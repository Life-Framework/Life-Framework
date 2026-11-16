//------------------------------------------------------------------------------------------------
//! Deploy: on the folded roadblock kit (Roadblock_Kit.et). Shown to on-duty police. The officer's
//! machine sends the spot its ghost shows as the action's own data (OnSaveActionData); the server
//! keeps it (OnLoadActionData) and PerformAction hands it to LF_Roadblocks.Deploy, which checks it
//! again (#1940). With no fresh proposal (a server-side perform) the server places at its own plan.
//! Same shape as LF_SpikeStripDeployAction.
//------------------------------------------------------------------------------------------------
class LF_RoadblockDeployAction : ScriptedUserAction
{
	//! How long a proposal that arrived with the action's data is good for, until its perform.
	static const int PROPOSAL_MS = 3000; // lf-fixed: network slack between the data and the perform, not a gameplay knob

	protected bool m_bProposal;
	protected vector m_vProposalOrigin;
	protected vector m_vProposalHeading;
	protected int m_iProposalTick;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Police"))
			return false;

		return LF_PoliceUtils.IsOnDutyCharacter(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!LF_PoliceUtils.IsOnDutyCharacter(user))
		{
			SetCannotPerformReason("#LF-Roadblock_PoliceOnly");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Police", "LF_RoadblockDeployAction"))
			return;

		if (!Replication.IsServer())
			return;

		bool fresh = m_bProposal && System.GetTickCount() - m_iProposalTick <= PROPOSAL_MS;
		m_bProposal = false;
		if (!fresh)
		{
			LF_Roadblocks.DeployOwnSpot(pUserEntity, pOwnerEntity);
			return;
		}

		vector proposal[4];
		LF_PlacementGeometry.Transform(m_vProposalOrigin - m_vProposalHeading * 0.5, m_vProposalOrigin + m_vProposalHeading * 0.5, proposal);
		LF_Roadblocks.Deploy(pUserEntity, pOwnerEntity, proposal);
	}

	//------------------------------------------------------------------------------------------------
	//! The performing player's machine: the spot their ghost shows, else the plan from where they stand.
	//! On a listen host the data does not travel, so the host keeps it here as well.
	override protected bool OnSaveActionData(ScriptBitWriter writer)
	{
		IEntity kit = GetOwner();
		vector origin;
		vector heading;
		string from = "ghost";
		if (!LF_RoadblockPreview.CurrentSpot(kit, origin, heading))
		{
			from = "plan";
			IEntity user = LF_Utils.GetLocalCharacter();
			if (!user || !kit || !LF_Component<LF_PlaceableComponent>.Find(kit))
				return false;

			vector spot[4];
			LF_Roadblocks.SpotFor(user, kit, spot);
			origin = spot[3];
			heading = spot[2];
		}

		writer.WriteVector(origin);
		writer.WriteVector(heading);
		LF_Debug.Info(LF_Roadblocks.FEATURE, string.Format("proposal sent from the %1: %2 heading %3", from, origin, heading));
		if (Replication.IsServer())
			Keep(origin, heading);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The server: the proposal the performing player sent, kept until PerformAction.
	override protected bool OnLoadActionData(ScriptBitReader reader)
	{
		vector origin;
		vector heading;
		if (!reader.ReadVector(origin) || !reader.ReadVector(heading))
		{
			LF_Debug.Warn(LF_Roadblocks.FEATURE, "proposal unreadable; the server will place at its own plan");
			return false;
		}

		if (!Replication.IsServer())
			return true;

		Keep(origin, heading);
		LF_Debug.Info(LF_Roadblocks.FEATURE, string.Format("proposal received: %1 heading %2", origin, heading));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void Keep(vector origin, vector heading)
	{
		m_bProposal = true;
		m_vProposalOrigin = origin;
		m_vProposalHeading = heading;
		m_iProposalTick = System.GetTickCount();
	}

	//------------------------------------------------------------------------------------------------
	//! Local player only: the ghost of the roadblock while Deploy is selected. The server decides again.
	protected override void OnActionSelected()
	{
		super.OnActionSelected();
		if (!LF_RoadblockPreview.Enabled() || !GetOwner())
			return;

		if (!CanBeShownScript(LF_Utils.GetLocalCharacter()))
			return;

		LF_RoadblockPreview.BeginFor(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnActionDeselected()
	{
		super.OnActionDeselected();
		LF_RoadblockPreview.End();
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Roadblock_Deploy";
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Pick up: on any piece of a deployed roadblock (Roadblock_Cone.et, Roadblock_Barrier.et). On-duty
//! police fold the whole set back into their kit.
//------------------------------------------------------------------------------------------------
class LF_RoadblockPickUpAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Police") || !GetOwner())
			return false;

		return LF_PoliceUtils.IsOnDutyCharacter(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!LF_PoliceUtils.IsOnDutyCharacter(user))
		{
			SetCannotPerformReason("#LF-Roadblock_PoliceOnly");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Police", "LF_RoadblockPickUpAction"))
			return;

		if (!Replication.IsServer())
			return;

		LF_Roadblocks.PickUp(pUserEntity, pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Roadblock_PickUp";
		return true;
	}
}
