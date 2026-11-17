// red-proof: RED-OK 2026-11-17, `tools/cli test --no-build --red LF_Test_IncidentDamageFlush,...` (hook skips the flush calls, as the unfixed code had none): `once the window has ended the held damage is flushed` and `the end of the burst is in the log` failed; restored, green.

// tier: LOGIC
//! incident/damage-flush (#2414): damage held back inside a sample window reaches the log without a
//! later hit. The end of a short burst is published once its window has ended, a death publishes the
//! victim's held damage before the death record, and the flush tick runs while the log is installed.
class LF_Test_IncidentDamageFlush : LF_Test
{
	override string GetName()
	{
		return "incident/damage-flush";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_IncidentRecord> before = LF_IncidentLog.ExportAll();
		LF_IncidentLog.ResetStatics();
		LF_IncidentLog.Install();
		ctx.True(LF_IncidentDamage.IsTicking(), "installing the log starts the damage flush tick");

		// A short burst: three hits inside a 5 s window. Only the first leaves at once.
		string pair = "91>92";
		LF_IncidentDamage.Hit(pair, "dmg-victim", "dmg-attacker", 10, Vector(5, 0, 5), "rifle", 100, 5);
		LF_IncidentDamage.Hit(pair, "dmg-victim", "dmg-attacker", 10, Vector(5, 0, 5), "rifle", 101, 5);
		LF_IncidentDamage.Hit(pair, "dmg-victim", "dmg-attacker", 10, Vector(5, 0, 5), "rifle", 102, 5);
		ctx.Equal(1, LF_IncidentLog.Count(), "only the first hit of the burst is logged by the hits themselves");

		int flushed = 0;
		if (!RedProof())
			flushed = LF_IncidentDamage.FlushDue(103, 5);
		ctx.Equal(0, flushed, "a window still open is not flushed");

		if (!RedProof())
			flushed = LF_IncidentDamage.FlushDue(106, 5);
		ctx.Equal(1, flushed, "once the window has ended the held damage is flushed");
		ctx.Equal(2, LF_IncidentLog.Count(), "the end of the burst is in the log");

		array<ref LF_IncidentRecord> damage = LF_IncidentLog.Query(vector.Zero, -1, 0, -1, LF_IncidentConfig.KIND_DAMAGE);
		if (damage.Count() == 2)
		{
			ctx.Equal(20, damage[1].m_iAmount, "the flushed record carries the two held hits");
			ctx.EqualStr("dmg-victim", damage[1].m_sSubjectUid, "it names the victim");
			ctx.EqualStr("dmg-attacker", damage[1].m_sOtherUid, "and the attacker");
			ctx.EqualStr("rifle", damage[1].m_sHow, "and the weapon");
		}

		int again = LF_IncidentDamage.FlushDue(200, 5);
		ctx.Equal(0, again, "a flushed window holds nothing, so a later tick adds nothing");
		ctx.Equal(2, LF_IncidentLog.Count(), "the log did not grow");

		// A death ends the fight: the held damage goes in before the death record.
		string pairTwo = "93>94";
		LF_IncidentDamage.Hit(pairTwo, "dmg-dead", "dmg-killer", 30, Vector(7, 0, 7), "pistol", 300, 5);
		LF_IncidentDamage.Hit(pairTwo, "dmg-dead", "dmg-killer", 40, Vector(7, 0, 7), "pistol", 301, 5);
		int countBefore = LF_IncidentLog.Count();
		LF_OnPlayerKilled killed = LF_OnPlayerKilled.Create("dmg-dead", "dmg-killer");
		killed.m_vPosition = Vector(7, 0, 7);
		killed.m_sWeapon = "pistol";
		LF_EventBus.Publish(killed);
		ctx.Equal(countBefore + 2, LF_IncidentLog.Count(), "the held hit and the death both reached the log");

		array<ref LF_IncidentRecord> all = LF_IncidentLog.Query(vector.Zero, -1, 0, -1, "");
		if (all.Count() == countBefore + 2)
		{
			LF_IncidentRecord held = all[all.Count() - 2];
			LF_IncidentRecord death = all[all.Count() - 1];
			ctx.EqualStr(LF_IncidentConfig.KIND_DAMAGE, held.m_sKind, "the held damage comes first");
			ctx.Equal(40, held.m_iAmount, "and it is what the victim took after the first hit");
			ctx.EqualStr(LF_IncidentConfig.KIND_DEATH, death.m_sKind, "the death record follows");
		}

		LF_IncidentLog.ResetStatics();
		ctx.False(LF_IncidentDamage.IsTicking(), "resetting the log stops the tick");
		LF_IncidentLog.ApplyAll(before);
	}
}
