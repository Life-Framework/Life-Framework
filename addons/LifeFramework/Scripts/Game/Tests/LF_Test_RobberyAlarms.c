// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_RobberyAlarms`: expecting kind "bank" for the van offence failed with `an unnamed robbery takes the kind that lists its offence (expected 'bank', got 'van')`; restored, green.
// tier: LOGIC
//! Robbery alarms (#740): which alarm kind a robbery sounds, with no world. The kind the robbed thing names
//! wins; otherwise the kind that lists the offence; otherwise none (the plain siren). A kind's loud twin is
//! used when nobody is on duty, and a kind with no loud twin falls back to its normal alarm.
class LF_Test_RobberyAlarms : LF_Test
{
	override string GetName()
	{
		return "police/robbery-alarms";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PoliceConfig config = new LF_PoliceConfig();
		config.m_aAlarmKinds = new array<ref LF_AlarmKind>();
		config.m_aAlarmKinds.Insert(Kind("bank", "bank.et", "bank-loud.et", {"#armory"}));
		config.m_aAlarmKinds.Insert(Kind("van", "van.et", "", {"#van"}));

		ctx.EqualStr("bank", config.ResolveAlarmKind("bank", ""), "a named kind is the kind");
		ctx.EqualStr("bank", config.ResolveAlarmKind("bank", "#van"), "the robbed thing's kind wins over the offence's");
		ctx.EqualStr("van", config.ResolveAlarmKind("", "#van"), "an unnamed robbery takes the kind that lists its offence");
		ctx.EqualStr("bank", config.ResolveAlarmKind("", "#armory"), "a second offence resolves to its own kind");
		ctx.EqualStr("", config.ResolveAlarmKind("", "#shoplifting"), "an offence no kind lists has no kind (the plain siren)");
		ctx.EqualStr("", config.ResolveAlarmKind("", ""), "no kind and no offence has no kind");

		ctx.EqualStr("bank.et", config.KindPrefab("bank", false), "the normal alarm of a kind");
		ctx.EqualStr("bank-loud.et", config.KindPrefab("bank", true), "nobody on duty sounds the kind's loud twin");
		ctx.EqualStr("van.et", config.KindPrefab("van", true), "a kind with no loud twin falls back to its normal alarm");
		ctx.EqualStr("", config.KindPrefab("jewellery", false), "an unknown kind has no prefab");
		ctx.EqualStr("", config.KindPrefab("", false), "no kind has no prefab");
	}

	//------------------------------------------------------------------------------------------------
	protected LF_AlarmKind Kind(string id, string prefab, string loud, array<string> offences)
	{
		LF_AlarmKind kind = new LF_AlarmKind();
		kind.m_sId = id;
		kind.m_sPrefab = prefab;
		kind.m_sLoudPrefab = loud;
		kind.m_aOffences = new array<string>();
		foreach (string offence : offences)
			kind.m_aOffences.Insert(offence);

		return kind;
	}
}
