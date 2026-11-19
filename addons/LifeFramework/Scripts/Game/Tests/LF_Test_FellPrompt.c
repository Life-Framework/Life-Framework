// red-proof: 2026-11-19, `--only LF_Test_FellPrompt` before LF_FellPrompt existed: the class and ApplyStamina were missing, so the test could not compile. `--red LF_Test_FellPrompt` skips the stamina cost, so the stamina check fails.
//! lumber/fell-prompt (#3554): the prompt helper follows the nearest standing fellable tree while an
//! axe is held. It does not move for an empty hand, a tree out of reach or a felled tree; the server's
//! ResolveTree finds a real tree at a named spot and nothing else; the stamina cost lowers the bar.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_FellPrompt : LF_Test
{
	protected static const ResourceName PINE = "Prefabs/Vegetation/Tree/t_pinus_sylvestris/t_pinus_sylvestris_2f.et";

	override string GetName()
	{
		return "lumber/fell-prompt";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_FelledTrees.ResetStatics();
		LF_FellPrompt.ResetStatics();
		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		IEntity tree;
		IEntity far;
		if (character)
			RunChecks(ctx, character, tree, far);

		LF_FellPrompt.ResetStatics();
		LF_TestCharacterFixture.Delete(tree);
		LF_TestCharacterFixture.Delete(far);
		LF_TestCharacterFixture.Delete(character);
		LF_FelledTrees.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunChecks(LF_TestContext ctx, IEntity character, out IEntity tree, out IEntity far)
	{
		LF_TreeFellingConfig config = LF_TreeFellingConfig.Get();
		ctx.NotNull(config, "the tree species config loads");
		if (!config)
			return;

		vector spot = character.GetOrigin() + "40 0 0";
		spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
		tree = LF_Utils.SpawnEntityPrefab(PINE, spot);
		far = LF_Utils.SpawnEntityPrefab(PINE, spot + "20 0 0");
		ctx.NotNull(tree, "the pine spawns");
		ctx.NotNull(far, "the far pine spawns");
		if (!tree || !far)
			return;

		character.SetOrigin(spot + "1.5 0 0");
		ctx.True(LF_FellPrompt.IsFellable(tree, config), "a pine is fellable");

		// An empty hand gets no prompt.
		LF_FellPrompt.Refresh(character, false);
		ctx.True(LF_FellPrompt.Target() == null, "no axe in hand: the prompt is on no tree");

		// With the axe, the prompt goes to the pine beside the player, not the one 20 m on.
		LF_FellPrompt.Refresh(character, true);
		ctx.True(LF_FellPrompt.Target() == tree, "axe in hand: the prompt is on the nearest pine");
		LF_FellAction action = LF_FellPrompt.Action();
		ctx.NotNull(action, "the helper carries an LF_FellAction");

		// Moving away takes the prompt off.
		character.SetOrigin(spot + "9 0 0");
		LF_FellPrompt.Refresh(character, true);
		ctx.True(LF_FellPrompt.Target() == null, "out of reach: the prompt is off the tree");

		// The server finds a real tree at a named spot, and only that.
		ctx.True(LF_FellPrompt.ResolveTree(tree.GetOrigin()) == tree, "ResolveTree finds the pine at its own spot");
		ctx.True(LF_FellPrompt.ResolveTree(tree.GetOrigin() + "5 0 0") == null, "ResolveTree finds nothing where no tree stands");

		// A felled tree gets no prompt and cannot be asked for again.
		character.SetOrigin(spot + "1.5 0 0");
		LF_FelledTrees.Hide(tree, LF_FellAction.KeyOfTree(tree));
		LF_FellPrompt.Refresh(character, true);
		ctx.True(LF_FellPrompt.Target() == null, "a felled tree gets no prompt");
		ctx.True(LF_FellPrompt.ResolveTree(tree.GetOrigin()) == null, "a felled tree is not resolved for a second fell");

		// The stamina cost lowers the bar (it runs on the owner's machine in play).
		CharacterStaminaComponent stamina = LF_Component<CharacterStaminaComponent>.Find(character);
		ctx.NotNull(stamina, "the character has a stamina bar");
		if (!stamina)
			return;

		stamina.AddStamina(1.0);
		if (!RedProof())
			LF_FellAction.ApplyStamina(character, 0.25);

		ctx.True(stamina.GetStamina() < 0.9, "the fell's stamina cost lowers the bar");
	}
}
