// red-proof: disabled the remembered-stand check in StandSpotClear (`if (false && ...)`) and ran
// `tools\cli test --no-build --only LF_Test_HarnessStandClear` (2026-11-12). Observed:
//   [LFTEST]   - two characters stood at one target are apart (0 m)
//   [LFTEST] SUMMARY tier=only passed=0 failed=1 total=1
// Restored it and the test passed.

// tier: WORLD
//! #522: stand puts a character on a clear, ground-snapped spot. Two characters sent to the same
//! target must not share a spot (overlapping capsules down one of them).
class LF_Test_HarnessStandClear : LF_Test
{
	override string GetName()
	{
		return "harness/stand-clear-spot";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity first = LF_ExploitGuardsWorldFixture.SpawnCharacter(ctx, "85 0 85", false);
		IEntity second = LF_ExploitGuardsWorldFixture.SpawnCharacter(ctx, "90 0 90", false);
		if (!first || !second)
			return;

		vector spotA;
		vector spotB;
		ctx.EqualStr("", LF_AgentHarness.ServerStand(first, LF_TestAnnex.ORG_DESK, true, spotA), "first stand succeeds");
		first.SetOrigin(spotA);
		ctx.EqualStr("", LF_AgentHarness.ServerStand(second, LF_TestAnnex.ORG_DESK, true, spotB), "second stand succeeds");
		second.SetOrigin(spotB);

		vector delta = spotA - spotB;
		delta[1] = 0;
		ctx.True(delta.Length() > 0.8, string.Format("two characters stood at one target are apart (%1 m)", delta.Length()));

		IEntity desk = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.ORG_DESK);
		if (desk)
			ctx.True(spotA[1] < desk.GetOrigin()[1] + 0.7, "the stand spot is on the floor, not at the target's height");

		DamageManagerComponent damage = DamageManagerComponent.Cast(second.FindComponent(DamageManagerComponent));
		if (damage)
			ctx.True(damage.GetHealth() >= damage.GetMaxHealth() - 0.01, "the second character is unharmed");

		SCR_EntityHelper.DeleteEntityAndChildren(first);
		SCR_EntityHelper.DeleteEntityAndChildren(second);
	}
}
