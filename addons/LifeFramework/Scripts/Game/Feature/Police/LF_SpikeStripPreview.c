//------------------------------------------------------------------------------------------------
//! Client: the ghost of the spike strip an officer is about to lay (#928, the placement foundation's
//! first prototype, docs/roadmap/building-foundation.md). While the officer has the folded strip's
//! Deploy action selected, a ghost of the deployed strip lies where LF_SpikeStrips.Deploy would put
//! it: across the road, green when the spot is valid, red when not. It runs the server's own plan
//! (LF_SpikeStrips.Plan) every TICK_MS, so the colour is the answer the server will give.
//! Behind a flag: only while the game mode has dev tools on (DebugWorld), until #928 ships it.
//------------------------------------------------------------------------------------------------
class LF_SpikeStripPreview
{
	static const int TICK_MS = 50; // lf-fixed: refresh period of a cosmetic ghost, not a gameplay knob

	protected static EntityID s_ItemId = EntityID.INVALID;
	protected static ResourceName s_sDeployedPrefab;
	protected static float s_fLength;
	protected static bool s_bRunning;
	protected static LF_EPlacementVerdict s_eVerdict = LF_EPlacementVerdict.NO_GROUND;
	protected static string s_sReason;

	//------------------------------------------------------------------------------------------------
	//! The prototype flag: the running game mode has dev tools on (LF_GameModeRoleplay
	//! m_bDevToolsEnabled; DebugWorld yes, the playtest town and a live server no).
	static bool Enabled()
	{
		LF_GameModeRoleplay gameMode = LF_GameModeRoleplay.Cast(GetGame().GetGameMode());
		return gameMode && gameMode.AreDevToolsEnabled();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: starts the ghost for the folded strip `item` (an LF_DeployableItemComponent).
	//! Returns the first verdict, or NO_GROUND with an error logged when there is nothing to show.
	static LF_EPlacementVerdict BeginFor(IEntity item)
	{
		LF_DeployableItemComponent deployable = LF_Component<LF_DeployableItemComponent>.Find(item);
		if (!deployable || deployable.GetDeployedPrefab().IsEmpty())
		{
			LF_Debug.Error(LF_SpikeStrips.FEATURE, "preview skipped: the item names no deployed prefab");
			return LF_EPlacementVerdict.NO_GROUND;
		}

		return Begin(deployable.GetDeployedPrefab(), deployable.GetDeployedLength(), item);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: starts (or retargets) the ghost of `deployedPrefab`, `length` metres long. With an
	//! `item`, the preview ends by itself when that item is gone (laid, or taken away).
	static LF_EPlacementVerdict Begin(ResourceName deployedPrefab, float length, IEntity item)
	{
		s_ItemId = EntityID.INVALID;
		if (item)
			s_ItemId = item.GetID();

		s_sDeployedPrefab = deployedPrefab;
		s_fLength = length;
		if (!s_bRunning)
		{
			s_bRunning = true;
			GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
			LF_Debug.Info(LF_SpikeStrips.FEATURE, "preview on");
		}

		Tick();
		return s_eVerdict;
	}

	//------------------------------------------------------------------------------------------------
	static void End()
	{
		if (s_bRunning)
			GetGame().GetCallqueue().Remove(Tick);

		if (s_bRunning || LF_PlacementGhost.IsShown())
			LF_Debug.Info(LF_SpikeStrips.FEATURE, "preview off");

		s_bRunning = false;
		s_ItemId = EntityID.INVALID;
		LF_PlacementGhost.Hide();
	}

	//------------------------------------------------------------------------------------------------
	//! Stops following the officer but leaves the ghost where it lies (the harness, so a side shot
	//! can move the camera body away without the ghost following it).
	static void Hold()
	{
		if (s_bRunning)
			GetGame().GetCallqueue().Remove(Tick);

		s_bRunning = false;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsRunning()
	{
		return s_bRunning;
	}

	//------------------------------------------------------------------------------------------------
	static LF_EPlacementVerdict GetVerdict()
	{
		return s_eVerdict;
	}

	//------------------------------------------------------------------------------------------------
	//! Why the last verdict was not VALID; empty when it was.
	static string GetReason()
	{
		return s_sReason;
	}

	//------------------------------------------------------------------------------------------------
	//! Called from LF_SpikeStrips.ResetStatics when a world ends.
	static void ResetStatics()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bRunning = false;
		s_ItemId = EntityID.INVALID;
		s_eVerdict = LF_EPlacementVerdict.NO_GROUND;
		s_sReason = "";
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		IEntity officer = LF_Utils.GetLocalCharacter();
		BaseWorld world = GetGame().GetWorld();
		IEntity item;
		if (world && s_ItemId != EntityID.INVALID)
			item = world.FindEntityByID(s_ItemId);

		// The officer is gone, or the strip was laid or taken away: nothing left to preview.
		if (!officer || !world || (s_ItemId != EntityID.INVALID && !item))
		{
			End();
			return;
		}

		vector transform[4];
		LF_PlacementProbe probe = new LF_PlacementProbe();
		LF_EPlacementVerdict verdict = LF_SpikeStrips.Plan(officer, s_fLength, item, transform, probe);
		if (verdict != s_eVerdict)
			LF_Debug.Info(LF_SpikeStrips.FEATURE, string.Format("preview %1 at %2 %3", LF_PlacementRules.Name(verdict), transform[3], LF_PlacementRules.Reason(verdict, probe, LF_SpikeStrips.MAX_STEP)));

		s_eVerdict = verdict;
		s_sReason = LF_PlacementRules.Reason(verdict, probe, LF_SpikeStrips.MAX_STEP);
		LF_PlacementGhost.Show(s_sDeployedPrefab, transform, verdict == LF_EPlacementVerdict.VALID);
	}
}
