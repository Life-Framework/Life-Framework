// red-proof: observed 2026-11-19 with `tools\cli test --no-build --red LF_Test_EventMemorial`: RED-OK (`RedProof()` skips the opening run, so no radio goes quiet and nobody is served or paid). Green with `--only`.
//! events/memorial (#3806): the memorial rite operation of an event phase. The first run of a phase
//! run switches off the radios of the configured slots within the ring (and puts back the ones it
//! switched off), serves each player within the radius once (a standing award per character), and a
//! later run serves only a late arrival; a new phase run counts everybody again. The place is a test
//! override, the people are fixtures, the toast and the bell RPC are off (`m_bQuiet`); the bell
//! strokes and the toast are proved by hand (the PR's manual steps).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_EventMemorial : LF_Test
{
	static const ResourceName RADIO_PREFAB = "{16C2DFD610CC7AFB}Prefabs/Atmosphere/LF_Radio.et";
	static const string MODULE_ID = "test_memorial";
	static const string SLOT = "test_memorial_radio";
	static const string AWARD = "test_memorial_award";
	static const string UID_A = "test-memorial-a";
	static const string UID_B = "test-memorial-b";
	static const string UID_C = "test-memorial-c";
	static const string UID_D = "test-memorial-d";
	static const float RADIUS = 30;
	static const float RADIO_RADIUS = 100;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "events/memorial";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EventMemorial Config()
	{
		LF_EventMemorial config = new LF_EventMemorial();
		config.m_sSceneId = "test_memorial_scene";
		config.m_fRadius = RADIUS;
		config.m_aRadioSlots = {};
		config.m_aRadioSlots.Insert(SLOT);
		config.m_fRadioRadius = RADIO_RADIUS;
		config.m_fSilenceSeconds = 300;
		config.m_iBellStrokes = 0;
		config.m_sAwardAction = AWARD;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EventMemorialOperation Operation(vector site)
	{
		LF_EventMemorialOperation operation = new LF_EventMemorialOperation(MODULE_ID, Config());
		operation.m_bSiteOverride = true;
		operation.m_vSiteOverride = site;
		operation.m_bQuiet = true;
		operation.m_aTestPeople = {};
		return operation;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ReputationConfig ReputationConfig()
	{
		LF_ReputationConfig config = new LF_ReputationConfig();
		array<int> thresholds = {-300, -100, 0, 100, 400, 1000};
		config.m_aFactions = {};
		config.m_aFactions.Insert(LF_ReputationFactionDef.Make("TOWN", thresholds));
		config.m_aAwards = {};
		config.m_aAwards.Insert(LF_ReputationAwardDef.Make(AWARD, "TOWN", 10));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_AmbientSwitchComponent Power(IEntity radio)
	{
		return LF_AmbientSwitchComponent.Cast(radio.FindComponent(LF_AmbientSwitchComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected static bool NameSlot(IEntity radio, string slot)
	{
		LF_AmbientEmitterComponent emitter = LF_AmbientEmitterComponent.Cast(radio.FindComponent(LF_AmbientEmitterComponent));
		if (!emitter)
			return false;

		emitter.SetSoundSlot(slot);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		if (!Replication.IsServer())
			return;

		// Config checks: a scene id and a radius are needed; radio slots need a ring and a time.
		LF_EventMemorial noScene = Config();
		noScene.m_sSceneId = "";
		ctx.False(noScene.Validate().IsEmpty(), "a memorial with no scene id is refused");
		LF_EventMemorial noRadius = Config();
		noRadius.m_fRadius = 0;
		ctx.False(noRadius.Validate().IsEmpty(), "a memorial with no radius is refused");
		LF_EventMemorial noSilence = Config();
		noSilence.m_fSilenceSeconds = 0;
		ctx.False(noSilence.Validate().IsEmpty(), "radio slots with no silence time are refused");
		ctx.True(Config().Validate().IsEmpty(), "a complete memorial is accepted");

		LF_EventModuleLoader.UnregisterLoaded();
		ctx.True(LF_EventModuleLoader.RegisterModule(MODULE_ID, Config(), "test"), "the loader takes a memorial module");
		ctx.True(LF_EventShell.IsRegistered(MODULE_ID), "the memorial is registered under its id");
		ctx.False(LF_EventModuleLoader.RegisterModule("test_memorial_bad", noScene, "test"), "the loader refuses a memorial with no scene");
		LF_EventModuleLoader.UnregisterLoaded();

		LF_ReputationConfig.UseForTest(ReputationConfig());
		LF_Reputation.ResetStatics();

		float x = 60;
		float z = 60;
		vector site = Vector(x, GetGame().GetWorld().GetSurfaceY(x, z) + 0.2, z);

		// Three radios: one in the ring on a named slot, one in the ring on another slot, one far.
		IEntity near = LF_ExploitGuardsWorldFixture.Spawn(RADIO_PREFAB, site + Vector(20, 0, 0));
		IEntity other = LF_ExploitGuardsWorldFixture.Spawn(RADIO_PREFAB, site + Vector(-20, 0, 0));
		IEntity far = LF_ExploitGuardsWorldFixture.Spawn(RADIO_PREFAB, site + Vector(RADIO_RADIUS + 60, 0, 0));
		ctx.True(near != null && other != null && far != null, "the three radios spawn");
		if (!near || !other || !far)
		{
			LF_ExploitGuardsWorldFixture.Despawn(near);
			LF_ExploitGuardsWorldFixture.Despawn(other);
			LF_ExploitGuardsWorldFixture.Despawn(far);
			return;
		}

		ctx.True(NameSlot(near, SLOT) && NameSlot(other, "test_memorial_other") && NameSlot(far, SLOT), "each radio has an emitter to name a slot on");
		ctx.True(Power(near).IsOn() && Power(other).IsOn() && Power(far).IsOn(), "all three radios start on");

		LF_EventMemorialOperation operation = Operation(site);
		operation.m_aTestPeople.Insert(LF_EventMemorialPerson.Make(1, UID_A, site + Vector(5, 0, 0)));
		operation.m_aTestPeople.Insert(LF_EventMemorialPerson.Make(2, UID_B, site + Vector(RADIUS + 25, 0, 0)));
		operation.m_aTestPeople.Insert(LF_EventMemorialPerson.Make(3, UID_C, site + Vector(0, 0, -10)));

		// 1. The opening run: radios quiet, two of the three people served and paid once.
		LF_EventContext phaseA = LF_EventContext.Make(12, 3, 2, 100);
		bool acted;
		if (!RedProof())
			acted = operation.Run("test_event", "memorial", phaseA);

		ctx.True(acted, "the opening run acted");
		ctx.Equal(LF_EMemorialOutcome.OPENED, operation.m_eLast, "the first run of a phase run opens the rite");
		ctx.False(Power(near).IsOn(), "the radio of the named slot in the ring goes quiet");
		ctx.True(Power(other).IsOn(), "a radio of another slot is left alone");
		ctx.True(Power(far).IsOn(), "a radio outside the ring is left alone");
		ctx.Equal(2, operation.m_iLastServed, "the two people within the radius are served");
		ctx.Equal(10, LF_Reputation.Points(UID_A, "TOWN"), "a person at the memorial gets the day's standing");
		ctx.Equal(0, LF_Reputation.Points(UID_B, "TOWN"), "a person outside the radius gets none");
		ctx.Equal(10, LF_Reputation.Points(UID_C, "TOWN"), "a second person at the memorial gets it too");

		// 2. The same phase run does not pay again; a late arrival is served.
		acted = operation.Run("test_event", "memorial", phaseA);
		ctx.False(acted, "a later run with nobody new does nothing");
		ctx.Equal(LF_EMemorialOutcome.QUIET, operation.m_eLast, "a later run with nobody new is quiet");
		ctx.Equal(10, LF_Reputation.Points(UID_A, "TOWN"), "standing is paid once per phase run");
		operation.m_aTestPeople.Insert(LF_EventMemorialPerson.Make(4, UID_D, site + Vector(8, 0, 8)));
		acted = operation.Run("test_event", "memorial", phaseA);
		ctx.True(acted, "a late arrival acts");
		ctx.Equal(LF_EMemorialOutcome.SERVED, operation.m_eLast, "a late arrival is served without a second opening");
		ctx.Equal(10, LF_Reputation.Points(UID_D, "TOWN"), "the late arrival gets the standing");
		ctx.False(Power(near).IsOn(), "the radio stays quiet through the phase run");

		// 3. The radios come back: the one switched off is on again, the others never moved.
		operation.RestoreRadios();
		ctx.True(Power(near).IsOn(), "the quiet radio is back on after the silence");
		ctx.True(Power(other).IsOn() && Power(far).IsOn(), "the radios never touched are still on");

		// 4. A radio a player had already switched off stays off after the silence.
		Power(near).SetOn(false);
		LF_EventContext phaseB = LF_EventContext.Make(12, 3, 2, 900);
		operation.Run("test_event", "memorial", phaseB);
		operation.RestoreRadios();
		ctx.False(Power(near).IsOn(), "a radio that was already off is not switched on by the rite");
		ctx.Equal(20, LF_Reputation.Points(UID_A, "TOWN"), "a new phase run pays everybody present again");

		// 5. A scene with no placed target: no rite.
		LF_EventMemorialOperation lost = new LF_EventMemorialOperation(MODULE_ID, Config());
		lost.m_bQuiet = true;
		lost.Run("test_event", "memorial", LF_EventContext.Make(12, 3, 2, 1200));
		ctx.Equal(LF_EMemorialOutcome.NO_SITE, lost.m_eLast, "no placed event_target: no site");

		operation.RestoreRadios();
		LF_ExploitGuardsWorldFixture.Despawn(near);
		LF_ExploitGuardsWorldFixture.Despawn(other);
		LF_ExploitGuardsWorldFixture.Despawn(far);
		LF_Reputation.ResetStatics();
		LF_ReputationConfig.Reload();
	}
}
