// red-proof: observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_SirenToggle`: removing the `mode == current`
// guard in LF_SirenRules.Judge failed it with `- the current mode is a no-op` (and the per-vehicle broadcast counts).
// Restored; green.

//------------------------------------------------------------------------------------------------
// tier: WORLD
//! vehicles/siren (#263): a siren toggle replicates once per change. A repeated mode, a mode out of
//! range and a request inside the throttle window broadcast nothing; the service vehicle prefabs
//! (police jeep, ambulance) carry the siren and the lights come on and off with the mode.
class LF_Test_SirenToggle : LF_Test
{
	protected static const ResourceName POLICE = "{EFEB161F9BC2031D}Prefabs/Vehicles/Wheeled/UAZ469/LF_UAZ469_Police.et";
	protected static const ResourceName AMBULANCE = "{D8C109C681E7AAF1}Prefabs/Vehicles/Wheeled/M997/LF_M997_Ambulance.et";

	override string GetName()
	{
		return "vehicles/siren";
	}

	override void Run(LF_TestContext ctx)
	{
		// The pure rule.
		ctx.True(LF_SirenRules.Judge(1, 4, 0, 1000, 0, false) == LF_ESirenRequest.ACCEPT, "a new mode is accepted");
		ctx.True(LF_SirenRules.Judge(0, 4, 0, 1000, 0, false) == LF_ESirenRequest.SAME, "the current mode is a no-op");
		ctx.True(LF_SirenRules.Judge(-1, 4, 0, 1000, 0, false) == LF_ESirenRequest.INVALID, "a negative mode is refused");
		ctx.True(LF_SirenRules.Judge(4, 4, 0, 1000, 0, false) == LF_ESirenRequest.INVALID, "a mode past the list is refused");
		ctx.True(LF_SirenRules.Judge(2, 4, 1, 1100, 1000, true) == LF_ESirenRequest.TOO_FAST, "a change inside the window is refused");
		ctx.True(LF_SirenRules.Judge(2, 4, 1, 1000 + LF_SirenRules.MIN_TOGGLE_MS, 1000, true) == LF_ESirenRequest.ACCEPT, "a change after the window is accepted");

		RunVehicle(ctx, POLICE, "police jeep");
		RunVehicle(ctx, AMBULANCE, "ambulance");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunVehicle(LF_TestContext ctx, ResourceName prefab, string label)
	{
		Resource res = Resource.Load(prefab);
		ctx.True(res.IsValid(), label + " prefab loads");
		if (!res.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		IEntity vehicle = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.NotNull(vehicle, label + " spawns");
		if (!vehicle)
			return;

		LF_SirenManagerComponent siren = LF_Component<LF_SirenManagerComponent>.Find(vehicle);
		ctx.NotNull(siren, label + " carries LF_SirenManagerComponent");
		if (siren)
		{
			ctx.False(siren.IsOn(), label + " starts with the siren off");
			ctx.Equal(0, siren.GetBroadcastCount(), label + " starts with no change broadcast");
			ctx.True(siren.ApplyRequest(1, 1000), label + " toggles on");
			ctx.True(siren.IsOn(), label + " siren runs after the toggle");
			ctx.False(siren.ApplyRequest(1, 1100), label + " the same mode asked twice is refused");
			ctx.False(siren.ApplyRequest(2, 1100), label + " a change inside the throttle window is refused");
			ctx.False(siren.ApplyRequest(99, 5000), label + " a mode past the list is refused");
			ctx.Equal(1, siren.GetBroadcastCount(), label + " one change, one broadcast");
			ctx.True(siren.ApplyRequest(0, 2000), label + " toggles off");
			ctx.False(siren.IsOn(), label + " siren is off again");
			ctx.Equal(2, siren.GetBroadcastCount(), label + " two changes, two broadcasts");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
	}
}
