// red-proof: observed 2026-11-16 with `tools\cli test --no-build --only LF_Test_RobberyAlarmKinds` on the unchanged LF_Police.conf: failed with `the hospital till resolves to the hospital kind by its offence (expected 'hospital', got '')`; with the hospital row it is green.
// tier: LOGIC
//! Robbery alarm kinds (#1937, chapter #1014), the shipped map in LF_Police.conf: each robbery kind maps to its
//! own alarm id and loud twin, the hospital till has the `hospital` kind, the chop shop maps to none (it files its
//! crime report with no site, so no alarm ever sounds), and an unknown kind falls back to the plain siren.
class LF_Test_RobberyAlarmKinds : LF_Test
{
	override string GetName()
	{
		return "alarm/robbery-kind-map";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PoliceConfig config = LF_PoliceConfig.Get();
		ctx.True(config != null, "the shipped police config loads");
		if (!config)
			return;

		array<string> kinds = {"bank", "fuel", "store", "van", "hospital"};
		array<string> prefabs = {};
		foreach (string id : kinds)
		{
			ctx.True(config.FindAlarmKind(id) != null, id + ": the kind is in LF_Police.conf");
			ResourceName normal = LF_PoliceConfig.AlarmPrefabFor(id, "", false);
			ResourceName loud = LF_PoliceConfig.AlarmPrefabFor(id, "", true);
			ctx.False(normal.IsEmpty(), id + ": it has a normal alarm");
			ctx.False(loud.IsEmpty(), id + ": it has a loud twin");
			ctx.True(normal != LF_PoliceConfig.AlarmPrefab(false), id + ": its alarm is not the plain siren");
			ctx.True(prefabs.Find(normal) < 0, id + ": its alarm is its own, no other kind shares the prefab");
			prefabs.Insert(normal);
		}

		ctx.EqualStr("hospital", config.ResolveAlarmKind("", LF_WantedReports.OFFENCE_HOSPITAL), "the hospital till resolves to the hospital kind by its offence");
		ctx.EqualStr("van", config.ResolveAlarmKind("", LF_WantedReports.OFFENCE_CASH_VAN), "the cash van keeps its own kind");
		ctx.EqualStr("bank", config.ResolveAlarmKind("", LF_WantedReports.OFFENCE_ARMORY), "the armory keeps the bank bell");

		// The chop shop is pinned to no alarm: no kind lists its offence, so it never resolves to one.
		ctx.EqualStr("", config.ResolveAlarmKind("", LF_WantedReports.OFFENCE_CHOP), "the chop shop maps to no alarm kind");
		foreach (string id : kinds)
		{
			LF_AlarmKind kind = config.FindAlarmKind(id);
			if (kind && kind.m_aOffences)
				ctx.True(kind.m_aOffences.Find(LF_WantedReports.OFFENCE_CHOP) < 0, id + ": it does not list the chop shop offence");
		}

		// An unknown kind falls back to the plain siren (the fallback logs, see LF_PoliceConfig.AlarmPrefabFor).
		ctx.True(LF_PoliceConfig.AlarmPrefabFor("jewellery", "", false) == LF_PoliceConfig.AlarmPrefab(false), "an unknown kind falls back to the plain siren");
		ctx.True(LF_PoliceConfig.AlarmPrefabFor("jewellery", "", true) == LF_PoliceConfig.AlarmPrefab(true), "and to the plain loud siren");
	}
}
