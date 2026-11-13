// red-proof: observed 2026-11-13 with `tools\cli test --no-build --only LF_Test_TaserSound`: with the M9 handling project put back in Handgun_Taser.et the test failed (`the taser plays its own handling project`, `no M9 sound project is left on the taser`); restored, green.
// tier: WORLD
//! The taser's sounds (#341, asset pipeline sound stage): the weapon prefab lists the taser's own
//! shot and handling sound projects in place of the vanilla M9's, the three projects load, the
//! stun flag a client reads follows the stun, and the buzz rule starts and stops on that flag.
//! That a sound is heard is a client matter (harness `sound`, and ears).
class LF_Test_TaserSound : LF_Test
{
	static const ResourceName TASER_PREFAB = "{6D583A40C5C5C150}Prefabs/Weapons/Handguns/Taser/Handgun_Taser.et";
	static const ResourceName SHOT_PROJECT = "{74C6FFC3E6765A20}Sounds/Weapons/Handguns/Taser/Weapons_Handguns_Taser_Shot.acp";
	static const ResourceName HANDLING_PROJECT = "{CA51DCAD77CCF499}Sounds/Weapons/Handguns/Taser/Weapons_Handguns_Taser_Handling.acp";

	override string GetName()
	{
		return "police/taser_sound";
	}

	override void Run(LF_TestContext ctx)
	{
		// The buzz rule.
		ctx.Equal(1, LF_StunBuzz.Decide(true, false), "a stunned character with no buzz starts one");
		ctx.Equal(-1, LF_StunBuzz.Decide(false, true), "a buzz on a character that is no longer stunned stops");
		ctx.Equal(0, LF_StunBuzz.Decide(true, true), "a buzz that plays is not started twice");
		ctx.Equal(0, LF_StunBuzz.Decide(false, false), "nothing to do for a character that is not stunned");

		RunProjects(ctx);
		RunPrefab(ctx);
		RunFlag(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunProjects(LF_TestContext ctx)
	{
		array<ResourceName> projects = {SHOT_PROJECT, HANDLING_PROJECT, LF_StunBuzz.PROJECT};
		foreach (ResourceName project : projects)
		{
			Resource res = Resource.Load(project);
			ctx.True(res && res.IsValid(), "the sound project loads: " + project);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The weapon's sound component lists the taser's projects and neither of the M9's own.
	protected void RunPrefab(LF_TestContext ctx)
	{
		Resource res = Resource.Load(TASER_PREFAB);
		if (!res || !res.IsValid())
		{
			ctx.Fail("the taser prefab does not load: " + TASER_PREFAB);
			return;
		}

		IEntitySource source = res.GetResource().ToEntitySource();
		array<ResourceName> files = {};
		for (int i = 0, count = source.GetComponentCount(); i < count; i++)
		{
			IEntityComponentSource component = source.GetComponent(i);
			if (component.GetClassName() == "WeaponSoundComponent")
				component.Get("Filenames", files);
		}

		ctx.True(files.Contains(SHOT_PROJECT), "the taser plays its own shot project");
		ctx.True(files.Contains(HANDLING_PROJECT), "the taser plays its own handling project");
		ctx.True(files.Count() >= 6, "the shared melee, pick-up and drop projects are still listed");
		foreach (ResourceName file : files)
		{
			ctx.False(file.Contains("Weapons_Handguns_M9_"), "no M9 sound project is left on the taser: " + file);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The flag clients sound the buzz on: set by a stun, cleared by its end, never set by a real downing.
	protected void RunFlag(LF_TestContext ctx)
	{
		LF_MedicConfig config = LF_MedicConfig.Get();
		bool wasEnabled = config.m_bDownedEnabled;
		config.m_bDownedEnabled = true;
		LF_Downed.SetTestAllEligible(true);

		IEntity entity = LF_TestCharacterFixture.Spawn(ctx);
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (character)
		{
			ctx.False(character.LF_HasStunFlag(), "a fresh character has no stun flag");
			ctx.True(LF_Stun.Apply(character, 5), "a stun is applied");
			ctx.True(character.LF_HasStunFlag(), "a stun sets the replicated stun flag");
			ctx.True(LF_Stun.End(character), "the stun ends");
			ctx.False(character.LF_HasStunFlag(), "the end of the stun clears the flag");
			character.LF_SetDowned(true, "");
			ctx.False(character.LF_HasStunFlag(), "a real downing does not set the stun flag");
			character.LF_SetDowned(false, "");
			LF_TestCharacterFixture.Delete(entity);
		}

		config.m_bDownedEnabled = wasEnabled;
		LF_Downed.SetTestAllEligible(false);
		LF_Stun.ResetStatics();
	}
}
