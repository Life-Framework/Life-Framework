// red-proof: observed 2026-11-09 with `tools\cli test --no-build --only LF_Test_Downed`: LF_Downed.Decide returning PASS instead of ABSORB for an already-downed player failed `the second hit was absorbed (expected 1, got 0)`; restored, green.

//------------------------------------------------------------------------------------------------
//! Records the LF_OnPlayerDowned events a test publishes.
class LF_TestDownedRecorder : LF_EventSubscriber
{
	string m_sLog;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnPlayerDowned downed = LF_OnPlayerDowned.Cast(busEvent);
		if (downed)
			m_sLog += downed.m_sPlayerUid + ">" + downed.m_sDownerUid + ";";

		return true;
	}
}

// tier: WORLD
//! Lethal damage to a player character leaves it alive and unconscious (the downed state), the
//! flag and the event say so, a downed body takes no further damage and does not wake, config off
//! kills as in vanilla, and Finish is the real death. Test characters have no player behind them,
//! so the test opts them in as eligible.
class LF_Test_Downed : LF_Test
{
	override string GetName()
	{
		return "medic/downed";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_MedicConfig config = LF_MedicConfig.Get();
		bool wasEnabled = config.m_bDownedEnabled;
		bool wasSearchable = config.m_bDownedCanBeSearched;
		LF_Downed.SetTestAllEligible(true);
		LF_EventBus.ResetStatics();
		LF_TestDownedRecorder recorder = new LF_TestDownedRecorder();
		LF_EventBus.Subscribe(LF_OnPlayerDowned, recorder);

		RunDowned(ctx, config, recorder);
		RunHeadshot(ctx, config);
		RunConfigOff(ctx, config);

		config.m_bDownedEnabled = wasEnabled;
		config.m_bDownedCanBeSearched = wasSearchable;
		LF_Downed.SetTestAllEligible(false);
		LF_EventBus.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDowned(LF_TestContext ctx, LF_MedicConfig config, LF_TestDownedRecorder recorder)
	{
		config.m_bDownedEnabled = true;
		config.m_bDownedCanBeSearched = true;
		IEntity entity = LF_TestCharacterFixture.Spawn(ctx);
		if (!entity)
			return;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(entity.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!character || !damage)
		{
			ctx.Fail("character lacks SCR_ChimeraCharacter or the character damage manager");
			LF_TestCharacterFixture.Delete(entity);
			return;
		}

		CharacterControllerComponent controller = character.GetCharacterController();
		ctx.False(character.LF_IsDowned(), "a fresh character is not downed");

		damage.Kill(Instigator.CreateInstigator(null));

		ctx.True(damage.GetState() != EDamageState.DESTROYED, "lethal damage leaves the character undestroyed");
		ctx.True(controller.GetLifeState() != ECharacterLifeState.DEAD, "lethal damage leaves the character alive");
		ctx.True(character.LF_IsDowned(), "the downed flag is set");
		ctx.True(damage.LF_WantsUnconscious(), "the downed character is held unconscious (the controller applies it next frame; the session proof reads the replicated life state)");
		ctx.EqualFloat(config.m_fDownedHealthScaled, damage.GetDefaultHitZone().GetHealthScaled(), 0.01, "the character is left with the configured sliver of health");
		ctx.Equal(1, LF_Downed.GetDownedCount(), "one downing was counted");
		ctx.EqualStr("npc>;", recorder.m_sLog, "LF_OnPlayerDowned was published once, with no downer");

		// No further damage, and nothing wakes it.
		damage.Kill(Instigator.CreateInstigator(null));
		ctx.True(controller.GetLifeState() != ECharacterLifeState.DEAD, "a second lethal hit does not kill a downed character");
		ctx.Equal(1, LF_Downed.GetAbsorbedCount(), "the second hit was absorbed");
		ctx.Equal(1, LF_Downed.GetDownedCount(), "a downed character is not downed twice");
		damage.UpdateConsciousness();
		ctx.True(damage.LF_WantsUnconscious(), "a consciousness update does not wake a downed character");

		// Searching a downed body follows the config.
		ctx.True(LF_Downed.AllowsAccess(entity, null), "a downed body can be searched while config allows it");
		config.m_bDownedCanBeSearched = false;
		ctx.False(LF_Downed.AllowsAccess(entity, null), "config closes the search of a downed body");
		config.m_bDownedCanBeSearched = true;

		// Revive path.
		ctx.True(LF_Downed.Clear(entity), "Clear ends the downed state");
		ctx.False(character.LF_IsDowned(), "the flag is cleared");
		ctx.False(damage.LF_WantsUnconscious(), "the cleared character is no longer held unconscious");

		// The real death: Finish kills a downed character.
		damage.Kill(Instigator.CreateInstigator(null));
		ctx.True(character.LF_IsDowned(), "downed again");
		ctx.True(LF_Downed.Finish(entity, Instigator.CreateInstigator(null)), "Finish runs");
		ctx.True(damage.GetState() == EDamageState.DESTROYED, "Finish kills a downed character for real");

		LF_TestCharacterFixture.Delete(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! The harness `shoot` shape: one lethal KINETIC hit on the head, which the damage manager
	//! passes on to the Health hit zone.
	protected void RunHeadshot(LF_TestContext ctx, LF_MedicConfig config)
	{
		config.m_bDownedEnabled = true;
		IEntity entity = LF_TestCharacterFixture.Spawn(ctx);
		if (!entity)
			return;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(entity.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!character || !damage || !damage.GetHeadHitZone())
		{
			ctx.Fail("character lacks SCR_ChimeraCharacter, the damage manager, or a head hit zone");
			LF_TestCharacterFixture.Delete(entity);
			return;
		}

		vector hitPosDirNorm[3];
		hitPosDirNorm[0] = entity.GetOrigin() + Vector(0, 1.6, 0);
		hitPosDirNorm[1] = vector.Forward;
		hitPosDirNorm[2] = vector.Up;
		SCR_DamageContext context = new SCR_DamageContext(EDamageType.KINETIC, 1000, hitPosDirNorm, entity, damage.GetHeadHitZone(), Instigator.CreateInstigator(null), null, -1, -1);
		damage.HandleDamage(context);

		ctx.True(damage.GetState() != EDamageState.DESTROYED, "a lethal head hit leaves the character undestroyed");
		ctx.True(character.LF_IsDowned(), "a lethal head hit downs the character");

		LF_TestCharacterFixture.Delete(entity);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunConfigOff(LF_TestContext ctx, LF_MedicConfig config)
	{
		config.m_bDownedEnabled = false;
		IEntity entity = LF_TestCharacterFixture.Spawn(ctx);
		if (!entity)
			return;

		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(entity.FindComponent(SCR_CharacterDamageManagerComponent));
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (!character || !damage)
		{
			ctx.Fail("character lacks SCR_ChimeraCharacter or the character damage manager");
			LF_TestCharacterFixture.Delete(entity);
			return;
		}

		damage.Kill(Instigator.CreateInstigator(null));
		ctx.True(damage.GetState() == EDamageState.DESTROYED, "with the downed state off, lethal damage kills");
		ctx.False(character.LF_IsDowned(), "with the downed state off, nobody is downed");

		LF_TestCharacterFixture.Delete(entity);
	}
}
