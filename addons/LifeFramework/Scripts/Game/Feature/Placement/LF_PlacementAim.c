//------------------------------------------------------------------------------------------------
//! Aimed placement mode (#1028, docs/roadmap/building-foundation.md section 4), the client side.
//! Entered from the placeable item's "Place where I aim" action (LF_PlacementAimAction). While it
//! runs, every TICK_MS the ghost lies where the camera aims, within the placeable's aim reach of the
//! player, turned to the player's yaw and snapped by the snap mode (LF_PlacementService.PlanAimed),
//! blue when the spot is valid and red with the reason in a hint when not. The keys (bound in
//! chimeraInputCommon.conf, listened to by LF_PlacementPlayerController): turn (15 degrees, or 1
//! with the fine key held), place, cancel. Place sends the ghost's spot to the server
//! (RpcAsk_LF_PlaceAimed), which confirms it as LF_PlacementService.Confirm always does; the ghost
//! is never replicated. The mode ends by itself when the player dies, goes down, gets in a vehicle,
//! or the item is gone or carried by someone else.
//------------------------------------------------------------------------------------------------
class LF_PlacementAim
{
	static const string FEATURE = "Placement";
	static const int TICK_MS = 50; // lf-fixed: refresh period of a cosmetic ghost, not a gameplay knob
	static const float HINT_SECONDS = 6; // lf-fixed: how long the keys hint shows, not a gameplay knob

	protected static EntityID s_ItemId = EntityID.INVALID;
	protected static bool s_bRunning;
	protected static bool s_bHeld;
	protected static float s_fYaw;
	protected static LF_EPlacementVerdict s_eVerdict = LF_EPlacementVerdict.NO_GROUND;
	protected static string s_sReason;
	protected static bool s_bClamped;
	protected static bool s_bSpot;
	protected static vector s_vSpotOrigin;
	protected static vector s_vSpotForward;
	protected static LF_EPlacementVerdict s_eHinted = LF_EPlacementVerdict.VALID;

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
	static float GetYaw()
	{
		return s_fYaw;
	}

	//------------------------------------------------------------------------------------------------
	static bool WasClamped()
	{
		return s_bClamped;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the ghost lies now (origin and forward), while the mode runs and a ghost is shown.
	static bool CurrentSpot(out vector origin, out vector forward)
	{
		if (!s_bRunning || !s_bSpot || !LF_PlacementGhost.IsShown())
			return false;

		origin = s_vSpotOrigin;
		forward = s_vSpotForward;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: enters the mode for `item` (an LF_PlaceableComponent the local character may place),
	//! the ghost starting along the character's facing. Entering again retargets to the new item.
	//! \return false with the reason logged when there is nothing to place or the player may not.
	static bool Begin(IEntity item)
	{
		IEntity character = LF_Utils.GetLocalCharacter();
		LF_PlaceableComponent placeable = LF_Component<LF_PlaceableComponent>.Find(item);
		if (!character || !placeable || placeable.GetPlacedPrefab().IsEmpty())
		{
			LF_Debug.Error(FEATURE, "aim skipped: no character, or the item names no placed prefab");
			return false;
		}

		if (!LF_PlacementService.MayPlace(character, placeable))
		{
			LF_Debug.Warn(FEATURE, "aim refused: the player may not place this item");
			return false;
		}

		vector transform[4];
		character.GetWorldTransform(transform);
		s_fYaw = LF_PlacementGeometry.WrapYaw(LF_PlacementGeometry.YawOf(transform[2]));
		s_ItemId = item.GetID();
		s_eHinted = LF_EPlacementVerdict.VALID;
		if (!s_bRunning)
		{
			s_bRunning = true;
			GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		}

		LF_Debug.Info(FEATURE, string.Format("aim on for %1, yaw %2", LF_PlacementRules.Describe(item), s_fYaw));
		LF_Utils.Notify("#LF-Placement_AimHint", "#LF-Placement_AimTitle", HINT_SECONDS);
		Tick();
		return s_bRunning;
	}

	//------------------------------------------------------------------------------------------------
	//! Turns the ghost `steps` times (negative is left), a step being 15 degrees, or 1 when `fine`.
	static void Turn(int steps, bool fine)
	{
		if (!s_bRunning)
			return;

		float degrees = LF_PlacementGeometry.STEP_DEGREES;
		if (fine)
			degrees = LF_PlacementGeometry.FINE_DEGREES;

		s_fYaw = LF_PlacementGeometry.TurnYaw(s_fYaw, steps, degrees);
		Tick();
	}

	//------------------------------------------------------------------------------------------------
	//! Freezes the ghost where it lies (the harness, so a world shot can move the camera body away
	//! without the ghost following it) or lets it follow the aim again.
	static void SetHeld(bool held)
	{
		s_bHeld = held;
	}

	//------------------------------------------------------------------------------------------------
	//! Plans the ghost again now (the harness reads it right after moving the camera).
	static void Refresh()
	{
		if (s_bRunning)
			Tick();
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the yaw outright (the harness, and the mode's own start).
	static void SetYaw(float yaw)
	{
		s_fYaw = LF_PlacementGeometry.WrapYaw(yaw);
		if (s_bRunning)
			Tick();
	}

	//------------------------------------------------------------------------------------------------
	//! Place: sends the ghost's spot to the server and leaves the mode. A red ghost is not sent: the
	//! hint names why and the mode keeps running so the player can move it.
	//! \return true when a proposal went out.
	static bool Place()
	{
		if (!s_bRunning)
			return false;

		BaseWorld world = GetGame().GetWorld();
		IEntity item;
		if (world)
			item = world.FindEntityByID(s_ItemId);

		vector origin;
		vector forward;
		if (!item || !CurrentSpot(origin, forward))
		{
			End("the item or the ghost is gone");
			return false;
		}

		if (s_eVerdict != LF_EPlacementVerdict.VALID)
		{
			LF_Debug.Warn(FEATURE, string.Format("place held back: the ghost is %1 (%2)", LF_PlacementRules.Name(s_eVerdict), s_sReason));
			ShowReason(s_eVerdict);
			return false;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			End("no player controller");
			return false;
		}

		LF_Debug.Info(FEATURE, string.Format("place: proposal %1 heading %2 sent", origin, forward));
		controller.LF_AskPlaceAimed(item, origin, forward);
		End("placed");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Cancel: leaves the mode and hides the ghost; nothing is sent.
	static void Cancel()
	{
		if (s_bRunning)
			End("cancelled");
	}

	//------------------------------------------------------------------------------------------------
	static void End(string why)
	{
		if (s_bRunning)
			GetGame().GetCallqueue().Remove(Tick);

		if (s_bRunning || LF_PlacementGhost.IsShown())
			LF_Debug.Info(FEATURE, "aim off: " + why);

		s_bRunning = false;
		s_bHeld = false;
		s_bSpot = false;
		s_ItemId = EntityID.INVALID;
		LF_PlacementGhost.Hide();
	}

	//------------------------------------------------------------------------------------------------
	//! Called from LF_StaticReset.ResetAll when a world ends.
	static void ResetStatics()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bRunning = false;
		s_bHeld = false;
		s_bSpot = false;
		s_bClamped = false;
		s_ItemId = EntityID.INVALID;
		s_eVerdict = LF_EPlacementVerdict.NO_GROUND;
		s_sReason = "";
		s_fYaw = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Why the mode must end now, or "" when the player and the item are still fit for it.
	protected static string LeaveReason(IEntity character, IEntity item)
	{
		if (!character)
			return "the character is gone";

		SCR_ChimeraCharacter chimera = SCR_ChimeraCharacter.Cast(character);
		if (!chimera)
			return "not a character";

		CharacterControllerComponent controller = chimera.GetCharacterController();
		if (!controller || controller.GetLifeState() != ECharacterLifeState.ALIVE)
			return "the character is dead or down";

		CompartmentAccessComponent access = chimera.GetCompartmentAccessComponent();
		if (access && access.IsInCompartment())
			return "the character is in a vehicle";

		if (!item)
			return "the item is gone";

		IEntity holder = LF_InventoryUtils.GetStorageHierachyRoot(item);
		if (holder != item && holder != character)
			return "the item is in someone else's storage";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected static void ShowReason(LF_EPlacementVerdict verdict)
	{
		string key = "#LF-Placement_NoGround";
		if (verdict == LF_EPlacementVerdict.BLOCKED)
			key = "#LF-Placement_Blocked";

		LF_Utils.Notify(key, "#LF-Placement_AimTitle", HINT_SECONDS);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		IEntity character = LF_Utils.GetLocalCharacter();
		BaseWorld world = GetGame().GetWorld();
		IEntity item;
		if (world && s_ItemId != EntityID.INVALID)
			item = world.FindEntityByID(s_ItemId);

		string leave = LeaveReason(character, item);
		LF_PlaceableComponent placeable = LF_Component<LF_PlaceableComponent>.Find(item);
		if (!leave.IsEmpty() || !placeable)
		{
			End(leave);
			return;
		}

		if (s_bHeld)
			return;

		CameraManager cameras = GetGame().GetCameraManager();
		CameraBase camera;
		if (cameras)
			camera = cameras.CurrentCamera();

		if (!camera)
			return;

		vector cameraTransform[4];
		camera.GetWorldTransform(cameraTransform);
		vector transform[4];
		LF_PlacementProbe probe = new LF_PlacementProbe();
		LF_EPlacementVerdict verdict = LF_PlacementService.PlanAimed(character, placeable, item, cameraTransform[3], cameraTransform[2], s_fYaw, transform, probe, s_bClamped);
		string reason = LF_PlacementRules.Reason(verdict, probe, LF_PlacementService.MAX_STEP);
		if (verdict != s_eVerdict)
			LF_Debug.Info(FEATURE, string.Format("aim ghost %1 at %2 %3", LF_PlacementRules.Name(verdict), transform[3], reason));

		s_eVerdict = verdict;
		s_sReason = reason;
		s_bSpot = LF_PlacementGhost.Show(placeable.GetPlacedPrefab(), transform, verdict == LF_EPlacementVerdict.VALID);
		s_vSpotOrigin = transform[3];
		s_vSpotForward = transform[2];

		// The reason when it turns red, once per change (the hint replaces the keys line).
		if (verdict != s_eHinted)
		{
			s_eHinted = verdict;
			if (verdict != LF_EPlacementVerdict.VALID)
				ShowReason(verdict);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! "Place where I aim": on the placeable item (the folded spike strip). Local only: it enters the
//! client's aimed placement mode; nothing is placed until the player confirms the ghost.
//------------------------------------------------------------------------------------------------
class LF_PlacementAimAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		LF_PlaceableComponent placeable = LF_Component<LF_PlaceableComponent>.Find(GetOwner());
		return placeable && LF_PlacementService.MayPlace(user, placeable);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		LF_PlaceableComponent placeable = LF_Component<LF_PlaceableComponent>.Find(GetOwner());
		if (!placeable || !LF_PlacementService.MayPlace(user, placeable))
		{
			SetCannotPerformReason("#LF-Placement_NotAllowed");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		LF_PlacementAim.Begin(pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Placement_Aim";
		return true;
	}
}
