// red-proof: 2026-11-23 `tools\cli test --no-build --red LF_Test_EveronRunPlateRow` RED-OK everon/run-plate-row (RedProof() skips the spawn of the S105, so the Cadet car carries no row and its assertion fails).
//! "Run plate" (#4425) on the rank fleet in game: under the Everon 1989 header (the pack leg of `tools\cli test`) both
//! police cars the pool hands out, the white S105 and the white UAZ469, carry an `LF_RunPlateAction` row. The S105's comes
//! from `tools\cli livery build` (the row's `actions`), the UAZ469's from its core base prefab `LF_UAZ469_Police.et`.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronRunPlateRow : LF_Test
{
	protected static const ResourceName SMALL_CAR = "{D1C459F87589D01D}Prefabs/Vehicles/Liveries/LF_S105_Police.et";
	protected static const ResourceName PATROL_CAR = "{C61DA18B80835A63}Prefabs/Vehicles/Liveries/LF_UAZ469_Police.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/run-plate-row";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity gate = LF_TestCharacterFixture.Spawn(ctx);
		if (!gate)
			return;

		vector at = gate.GetOrigin() + "8 0.5 0";
		IEntity small;
		if (!RedProof())
			small = LF_Utils.SpawnEntityPrefab(SMALL_CAR, at);

		ctx.True(CountRows(small) == 1, "the Cadet's S105 police car carries one Run plate row");
		LF_TestCharacterFixture.Delete(small);

		IEntity patrol = LF_Utils.SpawnEntityPrefab(PATROL_CAR, at);
		ctx.True(CountRows(patrol) == 1, "the Patrol UAZ469 police car carries one Run plate row");
		LF_TestCharacterFixture.Delete(patrol);
		LF_TestCharacterFixture.Delete(gate);
	}

	//------------------------------------------------------------------------------------------------
	protected int CountRows(IEntity car)
	{
		if (!car)
			return 0;

		ActionsManagerComponent actions = ActionsManagerComponent.Cast(car.FindComponent(ActionsManagerComponent));
		if (!actions)
			return 0;

		array<BaseUserAction> list = {};
		actions.GetActionsList(list);
		int count;
		foreach (BaseUserAction base : list)
		{
			if (LF_RunPlateAction.Cast(base))
				count++;
		}

		return count;
	}
}
