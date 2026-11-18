//------------------------------------------------------------------------------------------------
//! The fishing rod (#2930, docs/architecture/fishing.md sections 1 and 3): the rod prefab's hand tool
//! (tool id `rod`) with the owner's input. While it is in the hands of the local player, the fire
//! input casts at the water ahead, or strikes when a line is out. This component only asks: the cast
//! and the strike go to the server through the PlayerController bridge (`LF_AskFishingCast`,
//! `LF_AskFishingStrike`, in LF_FishingSession.c) and the server decides everything. What the server
//! answers comes back as `OnServerEvent` (the float and its dip hang off it).
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Fishing", description: "Fishing rod: the hand tool plus the owner's cast and strike input.")]
class LF_FishingRodComponentClass : LF_HandToolComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_FishingRodComponent : LF_HandToolComponent
{
	static const string FIRE_ACTION = "CharacterFire";
	static const float AIM_SHARE_OF_RANGE = 0.75;	// lf-fixed: the cast lands this share of the rod's range ahead
	static const float MIN_CAST_METRES = 3;			// lf-fixed: water nearer than this is at the angler's feet, not a cast (the float then sits in the view)

	//! Client: the id of the cast the server says is out, 0 when none. A strike carries it.
	protected static int s_iOpenCast;
	//! Client: the server told us the fish bit (the float dips; "Strike!" is on screen).
	protected static bool s_bBite;
	//! Client: where the last cast was aimed; the float goes there when the server says the line is out.
	protected static vector s_vPendingTarget;

	protected bool m_bListening;

	//------------------------------------------------------------------------------------------------
	override void OnModeChanged(EGadgetMode mode, IEntity charOwner)
	{
		super.OnModeChanged(mode, charOwner);

		bool local = charOwner && charOwner == SCR_PlayerController.GetLocalControlledEntity();
		if (mode == EGadgetMode.IN_HAND && local)
			Listen(true);
		else if (mode != EGadgetMode.IN_HAND)
		{
			Listen(false);
			LF_FishingFloat.Hide();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Listen(bool on)
	{
		if (on == m_bListening)
			return;

		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;

		if (on)
			input.AddActionListener(FIRE_ACTION, EActionTrigger.DOWN, OnFire);
		else
			input.RemoveActionListener(FIRE_ACTION, EActionTrigger.DOWN, OnFire);

		m_bListening = on;
	}

	//------------------------------------------------------------------------------------------------
	//! Local machine: the fire input with the rod in the hands.
	protected void OnFire()
	{
		Fire(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! Local machine: what the fire input does with the rod `rod` in the hands: strike when a line is
	//! out, else cast. The harness verb `fishing fire` calls it too (the listener is the only other caller).
	static void Fire(IEntity rod)
	{
		IEntity character = SCR_PlayerController.GetLocalControlledEntity();
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!character || !controller)
			return;

		if (s_iOpenCast > 0)
		{
			controller.LF_AskFishingStrike(s_iOpenCast);
			return;
		}

		s_vPendingTarget = CastPoint(character, rod);
		controller.LF_AskFishingCast(s_vPendingTarget);
	}

	//------------------------------------------------------------------------------------------------
	//! The flat point the cast lands on: the first water along the aim, else a share of the rod's range
	//! out. It is a request: the server checks the range, the water and everything else itself.
	protected static vector CastPoint(IEntity character, IEntity rod)
	{
		float reach = 15; // lf-fixed: used only when the config has no row for this rod
		LF_FishingConfig config = LF_FishingConfig.Get();
		if (config && rod)
		{
			LF_FishingRodDef def = config.FindRod(LF_Utils.GetPrefabName(rod));
			if (def)
				reach = def.m_fCastRange * AIM_SHARE_OF_RANGE;
		}

		vector ahead = character.GetTransformAxis(2);
		CameraBase camera;
		CameraManager cameras = GetGame().GetCameraManager();
		if (cameras)
			camera = cameras.CurrentCamera();

		if (camera)
		{
			vector view[4];
			camera.GetWorldTransform(view);
			ahead = view[2];
		}

		ahead[1] = 0;
		ahead.Normalize();

		// The first water along the aim, so a pond close by is cast at where it is; open ground out to
		// the full reach is still a request (the server refuses it as not water).
		vector origin = character.GetOrigin();
		for (float step = MIN_CAST_METRES; step <= reach; step += 1)
		{
			vector point = origin + ahead * step;
			if (!LF_FishingSession.WaterKindAt(point).IsEmpty())
				return point;
		}

		return origin + ahead * reach;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: what the server said about this player's cast (LF_EFishingEvent as an int).
	static void OnServerEvent(int kind, int castId)
	{
		if (kind == LF_EFishingEvent.CAST_OK)
		{
			s_iOpenCast = castId;
			s_bBite = false;
			LF_FishingFloat.Show(s_vPendingTarget);
		}
		else if (kind == LF_EFishingEvent.BITE)
		{
			s_iOpenCast = castId;
			s_bBite = true;
			LF_FishingFloat.Dip();
		}
		else
		{
			s_iOpenCast = 0;
			s_bBite = false;
			LF_FishingFloat.Hide();
		}

		LF_Debug.Info(LF_FishingConfig.FEATURE, string.Format("client: %1 (cast %2)", typename.EnumToString(LF_EFishingEvent, kind), castId));
	}

	//------------------------------------------------------------------------------------------------
	static bool IsLineOut()
	{
		return s_iOpenCast > 0;
	}

	//------------------------------------------------------------------------------------------------
	static bool HasBite()
	{
		return s_bBite;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_iOpenCast = 0;
		s_bBite = false;
		s_vPendingTarget = vector.Zero;
	}

	//------------------------------------------------------------------------------------------------
	void ~LF_FishingRodComponent()
	{
		if (m_bListening && GetGame() && GetGame().GetInputManager())
			Listen(false);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_FishingRodComponent's statics (LF_StaticReset finds it by reflection).
class LF_FishingRodComponent_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_FishingRodComponent";
	}

	override void Apply()
	{
		LF_FishingRodComponent.ResetStatics();
	}
}
