//! The drunk view (#1920): while the owner's impairment level is above zero their view sways and the
//! aim drifts with it, growing with the level and fading as it decays. Client only: the sway is
//! added to the local player's own aim input, so no other player's camera or aim is touched.
//------------------------------------------------------------------------------------------------
//! Tuning for the sway. Script defaults (no .conf yet); the members carry no initializers so a
//! future .conf overlay of 0 reads as 0 (#1077).
[BaseContainerProps(configRoot: true)]
class LF_ImpairmentViewConfig
{
	[Attribute("40", UIWidgets.Auto, "Impairment level at which the sway reaches its maximum; a higher level sways no more", params: "1 100")]
	float m_fFullLevel;

	[Attribute("1.5", UIWidgets.Auto, "Shape of the rise: 1 is straight, above 1 a light buzz stays gentle and a heavy one climbs fast")]
	float m_fCurvePower;

	[Attribute("1.6", UIWidgets.Auto, "Largest yaw drift in degrees at full impairment (pitch is 60% of it)")]
	float m_fMaxDegrees;

	[Attribute("0.35", UIWidgets.Auto, "Sway cycles per second at full impairment (half as fast with a light buzz)")]
	float m_fMaxCyclesPerSecond;

	//------------------------------------------------------------------------------------------------
	static LF_ImpairmentViewConfig CreateDefault()
	{
		LF_ImpairmentViewConfig config = new LF_ImpairmentViewConfig();
		config.m_fFullLevel = 40;
		config.m_fCurvePower = 1.5;
		config.m_fMaxDegrees = 1.6;
		config.m_fMaxCyclesPerSecond = 0.35;
		return config;
	}
}

//------------------------------------------------------------------------------------------------
//! The sway rule and the client driver. The rule is pure (LOGIC test `survival/impairment-view`); the
//! driver adds the rule's per-frame change to the local player's aim input.
class LF_ImpairmentView
{
	static const string LOG = "ImpairmentView";

	protected static ref LF_ImpairmentViewConfig s_Config;

	//! Harness / test override of the level the view reads; negative means use the replicated level.
	protected static float s_fDebugLevel = -1;

	//------------------------------------------------------------------------------------------------
	static LF_ImpairmentViewConfig GetConfig()
	{
		if (!s_Config)
			s_Config = LF_ImpairmentViewConfig.CreateDefault();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Peak sway in degrees for an impairment level (0-100): zero at level 0, following the config
	//! curve, never above the configured maximum.
	static float SwayAmplitude(float level, LF_ImpairmentViewConfig config)
	{
		if (level <= 0 || !config || config.m_fFullLevel <= 0)
			return 0;

		float fraction = Math.Clamp(level / config.m_fFullLevel, 0, 1);
		float amplitude = config.m_fMaxDegrees * Math.Pow(fraction, Math.Max(config.m_fCurvePower, 0.01));
		return Math.Clamp(amplitude, 0, config.m_fMaxDegrees);
	}

	//------------------------------------------------------------------------------------------------
	//! (yaw, pitch) drift in degrees at a moment. A slow wobble: two sines of different pace so it
	//! never settles into a plain circle. The pace slows to half for a light buzz.
	static vector SwayOffset(float level, float timeSeconds, LF_ImpairmentViewConfig config)
	{
		float amplitude = SwayAmplitude(level, config);
		if (amplitude <= 0)
			return vector.Zero;

		float strength = amplitude / Math.Max(config.m_fMaxDegrees, 0.001);
		float cycles = config.m_fMaxCyclesPerSecond * (0.5 + 0.5 * strength);
		float phase = timeSeconds * cycles * Math.PI2;
		float yaw = amplitude * Math.Sin(phase);
		float pitch = amplitude * 0.6 * Math.Sin(phase * 0.73 + 1.3);
		return Vector(yaw, pitch, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Harness: pins the level the view reads (0-100), or -1 to follow the replicated level again.
	static void SetDebugLevel(float level)
	{
		s_fDebugLevel = level;
		LF_Debug.Info(LOG, string.Format("view level pinned to %1", level));
	}

	//------------------------------------------------------------------------------------------------
	static float GetDebugLevel()
	{
		return s_fDebugLevel;
	}

	//------------------------------------------------------------------------------------------------
	//! The level the local view should show: the pin when set, else the replicated one.
	static float ViewLevel(int replicatedLevel)
	{
		if (s_fDebugLevel >= 0)
			return s_fDebugLevel;

		return replicatedLevel;
	}

	//------------------------------------------------------------------------------------------------
	//! World end: drop the pin.
	static void ResetStatics()
	{
		s_fDebugLevel = -1;
		s_Config = null;
	}
}

//------------------------------------------------------------------------------------------------
//! The owner's client frame: adds the change in sway since the last frame to the aim input, so the
//! view and the aim sway together and settle where the player left them once the level is gone.
modded class LF_CharacterSurvivalComponent
{
	protected float m_fViewTimeSeconds;
	protected vector m_vViewAppliedOffset;
	protected float m_fViewLogSeconds;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		SetEventMask(owner, EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);

		// Only the local player's own character: another player's body reads 0 here (owner-only RplProp)
		// and is skipped by the controlled-entity check, so no one else's view is touched.
		float level = LF_ImpairmentView.ViewLevel(GetReplicatedImpairment());
		if (level <= 0 && m_vViewAppliedOffset == vector.Zero)
			return;

		if (owner != SCR_PlayerController.GetLocalControlledEntity())
			return;

		CharacterControllerComponent controller = CharacterControllerComponent.Cast(owner.FindComponent(CharacterControllerComponent));
		if (!controller || controller.GetLifeState() != ECharacterLifeState.ALIVE)
			return;

		CharacterInputContext input = controller.GetInputContext();
		if (!input)
			return;

		m_fViewTimeSeconds += timeSlice;
		vector offset = LF_ImpairmentView.SwayOffset(level, m_fViewTimeSeconds, LF_ImpairmentView.GetConfig());
		vector delta = offset - m_vViewAppliedOffset;
		m_vViewAppliedOffset = offset;

		vector aimingNow = input.GetAimingAngles();
		vector aimingNext = aimingNow;
		aimingNext[0] = aimingNow[0] + delta[0] * Math.DEG2RAD;
		aimingNext[1] = aimingNow[1] + delta[1] * Math.DEG2RAD;
		input.SetAimingAngles(aimingNext);

		m_fViewLogSeconds += timeSlice;
		if (m_fViewLogSeconds >= 1)
		{
			m_fViewLogSeconds = 0;
			vector camAngles = vector.Zero;
			CameraManager cameras = GetGame().GetCameraManager();
			if (cameras && cameras.CurrentCamera())
			{
				vector camMat[4];
				cameras.CurrentCamera().GetWorldTransform(camMat);
				camAngles = Math3D.MatrixToAngles(camMat);
			}
			LF_Debug.Info(LF_ImpairmentView.LOG, string.Format("level %1 sway yaw %2 pitch %3 deg, aiming rad %4, camera deg %5", level, offset[0], offset[1], input.GetAimingAngles(), camAngles));
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Harness `impair <level>`: pins the local view level to prove the sway without a drink (`impair off`
//! follows the real level again).
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "impair")
			return super.Handle(cmd);

		if (cmd.Arg1.IsEmpty())
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "need_level_or_off");
			return true;
		}

		if (cmd.Arg1 == "off")
			LF_ImpairmentView.SetDebugLevel(-1);
		else
			LF_ImpairmentView.SetDebugLevel(Math.Clamp(cmd.Arg1.ToFloat(), 0, 100));

		LF_AgentHarness.Answer(cmd.Id, "ok", "view level " + LF_ImpairmentView.GetDebugLevel());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of this file's statics (Core/LF_StaticReset.c runs every feature's hook).
modded class LF_StaticResetHooks
{
	override void Run(notnull array<string> done, bool apply)
	{
		super.Run(done, apply);
		done.Insert("LF_ImpairmentView");
		if (apply)
			LF_ImpairmentView.ResetStatics();
	}
}
