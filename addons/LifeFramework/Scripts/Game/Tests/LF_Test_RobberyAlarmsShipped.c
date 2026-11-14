// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_RobberyAlarmsShipped`: flipping the range comparison failed with `the bank bell carries further than the store buzzer`; restored, green.
// tier: WORLD
//! Robbery alarms (#740), the shipped data: every robbery type (the bank vault, the fuel station till, the
//! general store till, the cash van, the police armory) resolves through LF_Police.conf to an alarm prefab
//! that loads and plays its own sound project and event; no two kinds share a sound; a normal and a loud
//! prefab of a kind play the same sound; the bank bell carries further than the store buzzer; and a robbery
//! with no kind (the hospital till) keeps the plain siren. That the sounds are heard is a manual step.
class LF_Test_RobberyAlarmsShipped : LF_Test
{
	override string GetName()
	{
		return "police/robbery-alarms-shipped";
	}

	override void Run(LF_TestContext ctx)
	{
		array<string> names = {"bank vault", "fuel till", "store till", "cash van", "armory"};
		array<string> kinds = {"bank", "fuel", "store", "", ""};
		array<string> offences = {"", "", "", LF_WantedReports.OFFENCE_CASH_VAN, LF_WantedReports.OFFENCE_ARMORY};
		array<string> projects = {};
		float bankRange = 0;
		float storeRange = 0;
		for (int i = 0; i < names.Count(); i++)
		{
			ResourceName normal = LF_PoliceConfig.AlarmPrefabFor(kinds[i], offences[i], false);
			ResourceName loud = LF_PoliceConfig.AlarmPrefabFor(kinds[i], offences[i], true);
			ctx.False(normal.IsEmpty(), names[i] + ": a normal alarm prefab resolves");
			ctx.False(loud.IsEmpty(), names[i] + ": a loud alarm prefab resolves");
			ctx.True(normal != LF_PoliceConfig.AlarmPrefab(false), names[i] + ": it is not the plain siren");

			ResourceName project;
			string soundEvent;
			float range;
			if (!Read(ctx, normal, names[i], project, soundEvent, range))
				continue;

			ctx.False(project.IsEmpty(), names[i] + ": the alarm names a sound project");
			ctx.False(soundEvent.IsEmpty(), names[i] + ": the alarm names a sound event");
			Resource audio = Resource.Load(project);
			ctx.True(audio && audio.IsValid(), names[i] + ": the sound project loads: " + project);
			if (names[i] == "bank vault")
				bankRange = range;
			else if (names[i] == "store till")
				storeRange = range;

			ResourceName loudProject;
			string loudSoundEvent;
			float loudRange;
			if (Read(ctx, loud, names[i] + " loud", loudProject, loudSoundEvent, loudRange))
				ctx.True(loudProject == project && loudSoundEvent == soundEvent, names[i] + ": the loud alarm plays the same sound");

			// The armory shares the bank's bell on purpose, so only the first of a pair is a new sound.
			if (names[i] != "armory")
			{
				ctx.True(projects.Find(project) < 0, names[i] + ": its sound is its own, no other kind plays it");
				projects.Insert(project);
			}
		}

		ctx.True(bankRange > storeRange, "the bank bell carries further than the store buzzer");

		// A robbery with no kind (the hospital till) keeps the plain siren.
		ctx.True(LF_PoliceConfig.AlarmPrefabFor("", LF_WantedReports.OFFENCE_HOSPITAL, false) == LF_PoliceConfig.AlarmPrefab(false), "an unlisted robbery keeps the plain siren");
		ctx.True(LF_PoliceConfig.AlarmPrefabFor("", LF_WantedReports.OFFENCE_HOSPITAL, true) == LF_PoliceConfig.AlarmPrefab(true), "and the plain loud siren");
	}

	//------------------------------------------------------------------------------------------------
	//! The alarm component's sound project, event and hear range as the prefab sets them.
	protected bool Read(LF_TestContext ctx, ResourceName prefab, string label, out ResourceName project, out string soundEvent, out float range)
	{
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			ctx.Fail(label + ": the alarm prefab does not load: " + prefab);
			return false;
		}

		IEntitySource source = res.GetResource().ToEntitySource();
		for (int i = 0, count = source.GetComponentCount(); i < count; i++)
		{
			IEntityComponentSource component = source.GetComponent(i);
			if (component.GetClassName() != "LF_SiteAlarmComponent")
				continue;

			component.Get("m_sSoundProject", project);
			component.Get("m_sSoundEvent", soundEvent);
			component.Get("m_fHearRange", range);
			return true;
		}

		ctx.Fail(label + ": the prefab has no LF_SiteAlarmComponent: " + prefab);
		return false;
	}
}
