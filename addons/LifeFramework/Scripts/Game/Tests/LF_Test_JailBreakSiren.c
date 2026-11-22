// red-proof: observed 2026-11-22 with `tools/cli test --no-build --red LF_Test_JailBreakSiren`: `RED-OK alarm/jailbreak-siren (fails with the hook on, 17 assertions)` (RedProof() skips naming the jail doors, so no target is a jail layer and no kind is set); green without it.
//! The jailbreak siren: both jail layers sound their own alarm kind ("jail" in LF_Police.conf, a prison klaxon with
//! its own prefabs and sound), no robbery kind shares its prefab, and a target outside the jail keeps its kind.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_JailBreakSiren : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "alarm/jailbreak-siren";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PoliceConfig config = LF_PoliceConfig.Get();
		ctx.True(config != null, "the shipped police config loads");
		if (!config)
			return;

		ctx.True(config.FindAlarmKind(LF_JailBreak.ALARM_KIND) != null, "the jail kind is in LF_Police.conf");
		ResourceName normal = LF_PoliceConfig.AlarmPrefabFor(LF_JailBreak.ALARM_KIND, "", false);
		ResourceName loud = LF_PoliceConfig.AlarmPrefabFor(LF_JailBreak.ALARM_KIND, "", true);
		ctx.False(normal.IsEmpty(), "the jail kind has a normal alarm");
		ctx.False(loud.IsEmpty(), "and a loud twin");
		ctx.True(normal != LF_PoliceConfig.AlarmPrefab(false), "its alarm is not the plain siren");
		array<string> others = {"bank", "fuel", "store", "van", "hospital", "shopbell"};
		foreach (string id : others)
			ctx.True(LF_PoliceConfig.AlarmPrefabFor(id, "", false) != normal, id + ": its alarm is not the jail siren");

		ctx.EqualStr(LF_JailBreak.ALARM_KIND, config.ResolveAlarmKind("", LF_JailBreak.OFFENCE_PRISON_BREACH), "the prison break offence resolves to the jail kind");

		string outerId = "jb-siren-outer";
		string cellId = "jb-siren-cell";
		string otherId = "jb-siren-other";
		LF_JailBreak.ResetForTest();
		LF_JailBreakConfig.UseForTest(LF_JailBreakConfig.CreateDefault());
		LF_Jail.Configure(40, 200, 1, 4);
		LF_Jail.SetCellOrigin("52 0 70");
		// RedProof(): the jail's doors are never named, so no target is a jail layer and no kind is set.
		if (!RedProof())
		{
			LF_Jail.SetDoorTarget(cellId);
			LF_Jail.SetOuterDoorTarget(outerId);
		}

		ctx.EqualStr(LF_JailBreak.ALARM_KIND, LF_JailBreak.AlarmKindFor(outerId, ""), "the outer door sounds the jailbreak siren");
		ctx.EqualStr(LF_JailBreak.ALARM_KIND, LF_JailBreak.AlarmKindFor(cellId, "bank"), "the cell door sounds it too, over any kind the door carried");
		ctx.EqualStr("bank", LF_JailBreak.AlarmKindFor(otherId, "bank"), "a door outside the jail keeps its own kind");
	}
}
