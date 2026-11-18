// red-proof: observed 2026-11-18 `tools/cli test --no-build --red LF_Test_BarberChangesHair,LF_Test_BarberPersist`: RED PROOF OK; barber/changes-hair fails `the haircut is accepted` (the paid haircut is skipped), barber/persist fails `the paid look is back after the load`. Green with `--only` (20 and 16 assertions).
//! The barber's chair (#3234): a haircut takes the fee from the cash carried, puts the look's head on the
//! live character and keeps it as the character's saved look; a refused haircut (no cash, the look worn,
//! an unknown look) charges nothing; the barber chair prefab carries both the seat and the barber action.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_BarberChangesHair : LF_Test
{
	static const string KEY = "test-barber-hair";
	static const ResourceName CHAIR = "{4F2A9C61B7E03D85}Prefabs/Props/Roleplay/LF_BarberChair.et";
	static const int FEE = 40;

	override string GetName()
	{
		return "barber/changes-hair";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		CheckChair(ctx);

		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		LF_Barber.Forget(KEY);
		CheckCut(ctx, character);
		LF_Barber.Forget(KEY);
		LF_TestCharacterFixture.Delete(character);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckCut(LF_TestContext ctx, IEntity character)
	{
		ResourceName before = LF_Barber.CurrentHead(character);
		ctx.False(before.IsEmpty(), "the roleplay character has a head");
		int look = LF_BarberSheet.Step(LF_Barber.IndexOf(before), 1, LF_Barber.Looks().Count());
		ResourceName wanted = LF_Barber.LookAt(look);
		ctx.True(wanted != before, "the next look is another head");

		// No cash: refused, nothing changes.
		LF_MoneyUtils.RemoveCash(character, LF_MoneyUtils.GetCash(character));
		ctx.Equal(LF_EBarberRefusal.NO_CASH, LF_Barber.Cut(character, KEY, look, FEE), "a haircut with no cash is refused");
		ctx.True(LF_Barber.CurrentHead(character) == before, "a refused haircut leaves the head");
		ctx.True(LF_Barber.SavedLook(KEY).IsEmpty(), "a refused haircut saves no look");

		// Unknown look: refused, nothing charged.
		LF_MoneyUtils.AddCash(character, 100);
		ctx.Equal(LF_EBarberRefusal.NO_LOOK, LF_Barber.Cut(character, KEY, 999, FEE), "an unknown look is refused");
		ctx.Equal(100, LF_MoneyUtils.GetCash(character), "an unknown look charges nothing");

		// The haircut. RedProof(): skipped, so every assertion about the paid look fails (`--red`).
		LF_EBarberRefusal paid = LF_EBarberRefusal.OFF;
		if (!RedProof())
			paid = LF_Barber.Cut(character, KEY, look, FEE);

		ctx.Equal(LF_EBarberRefusal.NONE, paid, "the haircut is accepted");
		ctx.Equal(100 - FEE, LF_MoneyUtils.GetCash(character), "the fee leaves the cash carried");
		ctx.True(LF_Barber.CurrentHead(character) == wanted, "the character wears the new look");
		ctx.True(LF_Barber.SavedLook(KEY) == wanted, "the saved look is the new head");

		// The same look again: refused, nothing charged.
		ctx.Equal(LF_EBarberRefusal.SAME_LOOK, LF_Barber.Cut(character, KEY, look, FEE), "the look already worn is refused");
		ctx.Equal(100 - FEE, LF_MoneyUtils.GetCash(character), "a refused repeat charges nothing");

		// A respawn puts the saved look back on a body that came with another head.
		LF_Barber.ApplyHead(character, before);
		ctx.True(LF_Barber.Restore(character, KEY), "a saved look is restored");
		ctx.True(LF_Barber.CurrentHead(character) == wanted, "the restored body wears the paid look");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckChair(LF_TestContext ctx)
	{
		Resource res = Resource.Load(CHAIR);
		ctx.True(res.IsValid(), "the barber chair prefab loads");
		if (!res.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = "5 0 5";
		IEntity chair = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.NotNull(chair, "the barber chair spawns");
		if (!chair)
			return;

		LF_BarberAction barber = LF_BarberAction.Of(chair);
		ctx.NotNull(barber, "the chair carries the barber action");
		if (barber)
			ctx.Equal(FEE, barber.GetFee(), "the chair's fee is 40");

		ctx.True(LF_ActionSiteHasSit(chair), "the chair still seats the customer");
		SCR_EntityHelper.DeleteEntityAndChildren(chair);
	}

	//------------------------------------------------------------------------------------------------
	protected bool LF_ActionSiteHasSit(IEntity chair)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(chair.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return false;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			if (LF_SitAction.Cast(action))
				return true;
		}

		return false;
	}
}
