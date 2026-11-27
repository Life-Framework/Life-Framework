// red-proof: observed 2026-11-28 with `tools\cli test --no-build --only LF_Test_ReviveContexts` on the unfixed prefab (both actions in LF_ID only): FAIL medic/revive-contexts, 16 lines "- Revive is offered in the default context" ... "- Load patient is offered in the RightArm context"; with the nine contexts listed on both actions it is green.
//! Revive and Load patient are offered wherever a medic's aim lands on a body (#5155). The in-game menu is the
//! actions of the one context the view ray is nearest to. LF_ID sits 1.3 m above the character origin, so on a body
//! lying on the ground a medic looking straight at it aimed the vanilla LowerTorso context (Loot, Inspect) and got no
//! Revive; only a medic looking a little above the body saw it. The two medic actions therefore belong to every body-part
//! context as well, where their Sort Priority (-10, -9) puts them first.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ReviveContexts : LF_Test
{
	override string GetName()
	{
		return "medic/revive-contexts";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		array<string> contexts = {"LF_ID", "default", "Head", "Chest", "LowerTorso", "LeftLeg", "RightLeg", "LeftArm", "RightArm"};
		BaseUserAction revive = LF_TestActionFinder.Find(character, LF_ReviveAction);
		BaseUserAction load = LF_TestActionFinder.Find(character, LF_LoadPatientAction);
		ctx.True(revive != null, "the player character carries LF_ReviveAction");
		ctx.True(load != null, "the player character carries LF_LoadPatientAction");

		foreach (string name : contexts)
		{
			if (revive)
				ctx.True(revive.GetContextIndex(name) >= 0, "Revive is offered in the " + name + " context");

			if (load)
				ctx.True(load.GetContextIndex(name) >= 0, "Load patient is offered in the " + name + " context");
		}

		LF_TestCharacterFixture.Delete(character);
	}
}
