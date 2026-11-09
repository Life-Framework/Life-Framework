// red-proof: observed 2026-11-09 with `tools\cli test --no-build --only LF_Test_FirstAid`: LF_FirstAid.Grant returning `add` (no cap) failed `the cap trims the tourniquet (expected 10, got 45)`; restored, green.

// tier: WORLD
//! First aid on a downed character: a bandage adds its seconds once (the same item is not counted
//! twice), a second item is trimmed by the cap and a third adds nothing, morphine adds nothing,
//! medic-only config refuses a civilian helper, and a character that is not downed gains nothing.
//! Spawned test characters stand in for the helper and for the items (only their ids are used).
class LF_Test_FirstAid : LF_Test
{
	override string GetName()
	{
		return "medic/first-aid";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_MedicConfig config = LF_MedicConfig.Get();
		bool wasEnabled = config.m_bDownedEnabled;
		bool wasAnyone = config.m_bFirstAidAnyone;
		float wasBleed = config.m_fBleedOutSeconds;
		float wasCap = config.m_fFirstAidCapSeconds;
		config.m_bDownedEnabled = true;
		config.m_bFirstAidAnyone = true;
		config.m_fBleedOutSeconds = 100;
		config.m_fFirstAidCapSeconds = 300;
		LF_Downed.ResetStatics();
		LF_Downed.SetTestAllEligible(true);
		LF_BleedOut.ResetStatics();

		IEntity patient = LF_TestCharacterFixture.Spawn(ctx);
		IEntity helper = LF_TestCharacterFixture.Spawn(ctx);
		IEntity itemB = LF_TestCharacterFixture.Spawn(ctx);
		IEntity itemC = LF_TestCharacterFixture.Spawn(ctx);
		if (patient && helper && itemB && itemC)
			RunAid(ctx, config, patient, helper, itemB, itemC);

		LF_TestCharacterFixture.Delete(patient);
		LF_TestCharacterFixture.Delete(helper);
		LF_TestCharacterFixture.Delete(itemB);
		LF_TestCharacterFixture.Delete(itemC);
		config.m_bDownedEnabled = wasEnabled;
		config.m_bFirstAidAnyone = wasAnyone;
		config.m_fBleedOutSeconds = wasBleed;
		config.m_fFirstAidCapSeconds = wasCap;
		LF_Downed.ResetStatics();
		LF_BleedOut.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunAid(LF_TestContext ctx, LF_MedicConfig config, IEntity patient, IEntity helper, IEntity itemB, IEntity itemC)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(patient);
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(patient.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!character || !damage)
		{
			ctx.Fail("character lacks SCR_ChimeraCharacter or the character damage manager");
			return;
		}

		ctx.EqualFloat(0, LF_FirstAid.Apply(patient, helper, itemB, LF_FirstAid.KIND_BANDAGE), 0.001, "a standing character gains nothing from first aid");

		damage.Kill(Instigator.CreateInstigator(null));
		ctx.True(character.LF_IsDowned(), "the lethal hit downs the character");
		ctx.EqualFloat(100, character.LF_GetBleedLeft(), 0.01, "the clock starts at the configured seconds");

		ctx.EqualFloat(0, LF_FirstAid.Apply(patient, helper, itemB, "morphine"), 0.001, "morphine adds nothing");

		// One bandage extends once: the same item reported twice counts once.
		ctx.EqualFloat(30, LF_FirstAid.Apply(patient, helper, itemC, LF_FirstAid.KIND_BANDAGE), 0.001, "a bandage adds its seconds");
		ctx.EqualFloat(130, character.LF_GetBleedLeft(), 0.01, "the server clock moved");
		ctx.Equal(130, character.LF_GetBleedSecondsLeft(), "the replicated copy moved with it");
		ctx.EqualFloat(0, LF_FirstAid.Apply(patient, helper, itemC, LF_FirstAid.KIND_BANDAGE), 0.001, "the same item is not counted twice");
		ctx.EqualFloat(130, character.LF_GetBleedLeft(), 0.01, "the clock did not move again");

		// The cap holds: 10 seconds of room trim a 45 second tourniquet, then nothing more fits.
		config.m_fFirstAidCapSeconds = 140;
		ctx.EqualFloat(10, LF_FirstAid.Apply(patient, helper, itemB, LF_FirstAid.KIND_TOURNIQUET), 0.001, "the cap trims the tourniquet");
		ctx.EqualFloat(140, character.LF_GetBleedLeft(), 0.01, "the clock stops at the cap");
		ctx.EqualFloat(0, LF_FirstAid.Apply(patient, helper, itemC, LF_FirstAid.KIND_SALINE), 0.001, "nothing is added at the cap");
		ctx.EqualFloat(140, character.LF_GetBleedLeft(), 0.01, "the clock held at the cap");

		// Medic only: a civilian helper adds no time.
		config.m_fFirstAidCapSeconds = 300;
		config.m_bFirstAidAnyone = false;
		ctx.EqualFloat(0, LF_FirstAid.Apply(patient, helper, patient, LF_FirstAid.KIND_SALINE), 0.001, "config medic only refuses a civilian helper");
		ctx.EqualFloat(140, character.LF_GetBleedLeft(), 0.01, "a refused helper leaves the clock alone");
	}
}
