// red-proof: 2026-11-23 RED-OK everon/police-car-rows, batched with LF_Test_PhonedPoliceCall: `tools/cli.mjs test --no-build --red LF_Test_PhonedPoliceCall,LF_Test_EveronPoliceCarRows` (RedProof() skips spawning the two cars, so no row is found).
//! everon/police-car-rows (#4606): the Cadet's S105 and the S1203 police van carry the police car's radio rows
//! like the UAZ469: "Radio: call resolved", "no one found" and "call unfounded" (LF_CallCloseAction), unit status
//! available and busy (LF_UnitStatusAction) and Run plate (LF_RunPlateAction). The rows come from the core police
//! base each generated livery prefab inherits, so `tools\cli livery build` keeps them.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronPoliceCarRows : LF_Test
{
	protected static const ResourceName S105 = "{D1C459F87589D01D}Prefabs/Vehicles/Liveries/LF_S105_Police.et";
	protected static const ResourceName S1203 = "{2603EE77C55F89BC}Prefabs/Vehicles/Liveries/LF_S1203_Police.et";

	override string GetName()
	{
		return "everon/police-car-rows";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Check(ctx, S105, "S105", "0 0 0");
		Check(ctx, S1203, "S1203", "8 0 0");
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, ResourceName prefab, string name, vector offset)
	{
		// Away from the gate, so no fixture character stands in it.
		vector at = "30 0.5 30" + offset;
		SCR_SpawnPoint spawnPoint = SCR_SpawnPoint.GetRandomSpawnPointForFaction("CIVILIAN");
		if (spawnPoint)
			at = spawnPoint.GetOrigin() + at;

		IEntity car;
		if (!RedProof())
			car = LF_Utils.SpawnEntityPrefab(prefab, at);

		ctx.NotNull(car, "the police " + name + " spawns");
		if (!car)
			return;

		ActionsManagerComponent actions = ActionsManagerComponent.Cast(car.FindComponent(ActionsManagerComponent));
		array<BaseUserAction> list = {};
		if (actions)
			actions.GetActionsList(list);

		int resolved, noOneFound, unfounded, available, busy, runPlate;
		foreach (BaseUserAction base : list)
		{
			LF_CallCloseAction close = LF_CallCloseAction.Cast(base);
			if (close && close.Outcome() == LF_CallOutcomeKind.RESOLVED)
				resolved++;
			else if (close && close.Outcome() == LF_CallOutcomeKind.NO_ONE_FOUND)
				noOneFound++;
			else if (close && close.Outcome() == LF_CallOutcomeKind.UNFOUNDED)
				unfounded++;

			LF_UnitStatusAction status = LF_UnitStatusAction.Cast(base);
			if (status && status.IsBusyRow())
				busy++;
			else if (status)
				available++;

			if (LF_RunPlateAction.Cast(base))
				runPlate++;
		}

		ctx.Equal(1, resolved, "the " + name + " has one 'Radio: call resolved' row");
		ctx.Equal(1, noOneFound, "the " + name + " has one 'Radio: no one found' row");
		ctx.Equal(1, unfounded, "the " + name + " has one 'Radio: call unfounded' row");
		ctx.Equal(1, available, "the " + name + " has one 'available' unit status row");
		ctx.Equal(1, busy, "the " + name + " has one 'busy' unit status row");
		ctx.Equal(1, runPlate, "the " + name + " has one Run plate row");
		LF_TestCharacterFixture.Delete(car);
	}
}
