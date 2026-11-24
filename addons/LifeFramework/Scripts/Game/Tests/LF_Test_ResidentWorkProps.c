// red-proof: 2026-11-24 (#4644), with the act skipped (RedProof() on, `cli test --no-build --red LF_Test_ResidentWorkProps`)
//   no prop is spawned, so `the carry pose spawns one prop` and `the sweep pose spawns one prop` fail.
//   RED-OK residents/work-props (fails with the hook on, 15 assertions).
//! residents/work-props (#4644): a carry pose holds a box and a sweep pose a broom, and a hammer pose holds
//! nothing. The loiter data the work loop starts a pose with names the prop (an item preset the base game
//! replicates with the data, so every machine that shows the pose spawns it locally and a late joiner too).
//! With a character: spawning the pose's items puts one prop on the hand bone, and ending the pose releases it.
//! The command itself runs on the next frames, so the test calls the spawn the command makes.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ResidentWorkProps : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "residents/work-props";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunRule(ctx);

		IEntity worker = LF_TestCharacterFixture.Spawn(ctx);
		if (worker)
			RunWorker(ctx, worker);

		LF_TestCharacterFixture.Delete(worker);
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_LoiterCustomAnimData DataFor(string job)
	{
		LF_ResidentWorkState state = new LF_ResidentWorkState();
		state.m_sActivity = job;
		state.m_sPose = LF_ResidentWorkLoop.PoseOf(job);
		return LF_ResidentWorkLoop.DataOf(state);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRule(LF_TestContext ctx)
	{
		ctx.True(DataFor("carry").m_eItemPresetID == SCR_EAnimationItemID.LF_WORK_CARRY, "a carry pose starts with the box preset");
		ctx.True(DataFor("sweep").m_eItemPresetID == SCR_EAnimationItemID.LF_WORK_SWEEP, "a sweep pose starts with the broom preset");
		ctx.True(DataFor("hammer").m_eItemPresetID == SCR_EAnimationItemID.NONE, "a hammer pose holds nothing");
		ctx.True(LF_ResidentWorkProps.Item(SCR_EAnimationItemID.NONE) == null, "the empty preset has no item");
		ctx.NotNull(LF_ResidentWorkProps.Item(SCR_EAnimationItemID.LF_WORK_CARRY), "the box preset has item data");
		ctx.NotNull(LF_ResidentWorkProps.Item(SCR_EAnimationItemID.LF_WORK_SWEEP), "the broom preset has item data");
		ctx.True(Resource.Load(LF_ResidentWorkProps.BOX_PREFAB).IsValid(), "the box prefab loads");
		ctx.True(Resource.Load(LF_ResidentWorkProps.BROOM_PREFAB).IsValid(), "the broom prefab loads");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWorker(LF_TestContext ctx, IEntity worker)
	{
		// The carrier's box.
		SCR_LoiterCustomAnimData carry = DataFor("carry");
		if (!RedProof())
			carry.SpawnItem(worker);

		array<IEntity> items = carry.GetItems();
		int count;
		if (items)
			count = items.Count();

		ctx.Equal(1, count, "the carry pose spawns one prop");
		if (count == 1)
		{
			ctx.True(items[0].GetParent() == worker, "the box hangs from the worker");
			ctx.True(items[0].GetPivot() == worker.GetAnimation().GetBoneIndex(LF_ResidentWorkProps.HAND_BONE), "the box hangs from the hand bone");
			ctx.True(items[0].GetPrefabData() && LF_Utils.SamePrefab(items[0].GetPrefabData().GetPrefabName(), LF_ResidentWorkProps.BOX_PREFAB), "the prop is the box");
		}

		carry.DeleteItem();
		ctx.True(carry.GetItems() == null, "ending the pose releases its props");

		// The sweeper's broom.
		SCR_LoiterCustomAnimData sweep = DataFor("sweep");
		if (!RedProof())
			sweep.SpawnItem(worker);

		items = sweep.GetItems();
		count = 0;
		if (items)
			count = items.Count();

		ctx.Equal(1, count, "the sweep pose spawns one prop");
		if (count == 1)
			ctx.True(items[0].GetPrefabData() && LF_Utils.SamePrefab(items[0].GetPrefabData().GetPrefabName(), LF_ResidentWorkProps.BROOM_PREFAB), "the prop is the broom");

		sweep.DeleteItem();
		ctx.True(sweep.GetItems() == null, "ending the sweep releases its prop");

		// A hammer pose spawns nothing.
		SCR_LoiterCustomAnimData hammer = DataFor("hammer");
		hammer.SpawnItem(worker);
		ctx.True(hammer.GetItems() == null, "the hammer pose spawns no prop");
	}
}
