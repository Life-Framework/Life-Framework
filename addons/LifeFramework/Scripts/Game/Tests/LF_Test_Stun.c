// red-proof: observed 2026-11-12 with `tools\cli test --no-build --only LF_Test_Stun`: LF_Stun.Wake without the token check failed `a stale wake leaves the stun in place`; restored, green.
// tier: WORLD
//! A stun (#341) puts a player character in the downed state without touching its health, the
//! player cannot give up out of it, the timed wake ends it, a stale wake does nothing, and a
//! character that is already downed is not stunned. The taser prefab loads and is a stun weapon.
//! Test characters have no player behind them, so the test opts them in as eligible.
class LF_Test_Stun : LF_Test
{
	static const ResourceName TASER_PREFAB = "{6D583A40C5C5C150}Prefabs/Weapons/Handguns/Taser/Handgun_Taser.et";

	override string GetName()
	{
		return "police/stun";
	}

	override void Run(LF_TestContext ctx)
	{
		// The pure rule.
		ctx.Equal(LF_Stun.PASS, LF_Stun.Decide(false, 1, 10, true), "a hit from an ordinary weapon passes to vanilla");
		ctx.Equal(LF_Stun.STUN, LF_Stun.Decide(true, 5, 10, true), "a stun weapon's hit in reach stuns");
		ctx.Equal(LF_Stun.ABSORB, LF_Stun.Decide(true, 11, 10, true), "a stun weapon's hit out of reach does nothing");
		ctx.Equal(LF_Stun.ABSORB, LF_Stun.Decide(true, 5, 10, false), "a stun weapon's hit on someone who cannot go down does nothing");

		LF_MedicConfig config = LF_MedicConfig.Get();
		bool wasEnabled = config.m_bDownedEnabled;
		config.m_bDownedEnabled = true;
		LF_Downed.SetTestAllEligible(true);
		LF_Stun.ResetStatics();

		RunStun(ctx);
		RunPrefab(ctx);

		config.m_bDownedEnabled = wasEnabled;
		LF_Downed.SetTestAllEligible(false);
		LF_Stun.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunStun(LF_TestContext ctx)
	{
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

		float healthBefore = damage.GetDefaultHitZone().GetHealthScaled();
		ctx.True(LF_Stun.Apply(character, 5), "a stun is applied");
		ctx.True(character.LF_IsDowned(), "a stunned character carries the downed flag");
		ctx.True(LF_Stun.IsStunned(entity), "the character is stunned");
		ctx.True(damage.LF_WantsUnconscious(), "a stunned character is held unconscious");
		ctx.EqualFloat(healthBefore, damage.GetDefaultHitZone().GetHealthScaled(), 0.001, "a stun deals no damage");
		ctx.Equal(1, LF_Stun.GetStunCount(), "one stun was counted");
		ctx.False(LF_Stun.Apply(character, 5), "a stunned character is not stunned again");
		ctx.False(LF_BleedOut.GiveUp(entity), "a stunned player cannot give up");
		ctx.True(damage.GetState() != EDamageState.DESTROYED, "the refused give-up killed nobody");

		// A wake from an older stun does nothing; the current one ends it.
		int token = character.LF_GetStunToken();
		LF_Stun.Wake(character, token - 1);
		ctx.True(LF_Stun.IsStunned(entity), "a stale wake leaves the stun in place");
		LF_Stun.Wake(character, token);
		ctx.False(character.LF_IsDowned(), "the timed wake clears the downed flag");
		ctx.False(damage.LF_WantsUnconscious(), "the woken character is no longer held unconscious");
		ctx.EqualFloat(healthBefore, damage.GetDefaultHitZone().GetHealthScaled(), 0.001, "the character wakes with the health it had");

		// A revive ends a stun early, and the old wake must not touch what comes after.
		ctx.True(LF_Stun.Apply(character, 5), "a second stun is applied");
		int second = character.LF_GetStunToken();
		ctx.True(LF_Downed.Clear(entity), "a revive ends a stun");
		ctx.False(LF_Stun.IsStunned(entity), "the revived character is not stunned");
		damage.Kill(Instigator.CreateInstigator(null));
		ctx.True(character.LF_IsDowned(), "lethal damage downs the character for real");
		LF_Stun.Wake(character, second);
		ctx.True(character.LF_IsDowned(), "the old stun's wake does not wake a really downed character");
		ctx.False(LF_Stun.Apply(character, 5), "a downed character is not stunned");

		LF_Downed.Finish(entity, Instigator.CreateInstigator(null));
		LF_TestCharacterFixture.Delete(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! The taser is a pistol with the stun marker and its own magazine well.
	protected void RunPrefab(LF_TestContext ctx)
	{
		Resource res = Resource.Load(TASER_PREFAB);
		if (!res || !res.IsValid())
		{
			ctx.Fail("the taser prefab does not load: " + TASER_PREFAB);
			return;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		IEntity taser = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!taser)
		{
			ctx.Fail("the taser prefab does not spawn");
			return;
		}

		LF_StunWeaponComponent stun = LF_StunWeaponComponent.Cast(taser.FindComponent(LF_StunWeaponComponent));
		ctx.True(stun != null, "the taser carries LF_StunWeaponComponent");
		if (stun)
		{
			ctx.True(stun.GetStunSeconds() > 0, "the taser's stun lasts");
			ctx.True(stun.GetRange() > 0, "the taser has a reach");
		}

		WeaponComponent weapon = WeaponComponent.Cast(taser.FindComponent(WeaponComponent));
		ctx.True(weapon != null, "the taser is a weapon");
		if (weapon)
		{
			BaseMuzzleComponent current = weapon.GetCurrentMuzzle();
			ctx.True(current != null, "the taser has a muzzle");
			if (current)
			{
				ctx.True(LF_MagazineWellTaser.Cast(current.GetMagazineWell()) != null, "the taser takes only taser cartridges");
				ctx.Equal(1, current.GetMaxAmmoCount(), "one cartridge, one shot");
			}
		}

		VObject mesh = taser.GetVObject();
		ctx.True(mesh != null, "the taser has a mesh");
		if (mesh)
		{
			TNodeId muzzleBone = taser.GetAnimation().GetBoneIndex("barrel_muzzle");
			ctx.True(muzzleBone != -1, "the mesh has the barrel_muzzle socket the pistol prefab names");
			ctx.True(taser.GetAnimation().GetBoneIndex("w_trigger") != -1, "the mesh has the vanilla pistol's w_trigger bone");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(taser);
	}
}
