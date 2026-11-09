// red-proof: Observed 2026-11-09 with `tools/cli test --no-build --only`: array.Remove(0) in LF_IncidentLog.Trim swaps the last record in, so the oldest-first check failed (`the oldest went first (expected 3, got 4)`); fixed with RemoveOrdered; green.

// tier: LOGIC
//! incident/log: the log appends, is bounded by count and retention, queries by radius and time
//! window, only records kinds the config allows, samples damage instead of logging every hit,
//! and fills itself from the event bus.
class LF_Test_IncidentLog : LF_Test
{
	override string GetName()
	{
		return "incident/log";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_IncidentRecord> before = LF_IncidentLog.ExportAll();
		LF_IncidentLog.ResetStatics();

		// Append and query by radius, window, and kind.
		LF_IncidentLog.Append(LF_IncidentConfig.KIND_DEATH, 1000, "victim-a", "killer-a", Vector(100, 0, 100), 0, "rifle", true);
		LF_IncidentLog.Append(LF_IncidentConfig.KIND_DAMAGE, 1010, "victim-a", "killer-a", Vector(103, 0, 104), 12, "rifle", false);
		LF_IncidentLog.Append(LF_IncidentConfig.KIND_CASH, 2000, "payer", "", Vector(500, 0, 500), 250, "atm-deposit", false);
		ctx.Equal(3, LF_IncidentLog.Count(), "three incidents were appended");

		array<ref LF_IncidentRecord> near = LF_IncidentLog.Query(Vector(100, 0, 100), 10, 0, -1, "");
		ctx.Equal(2, near.Count(), "a 10 m radius finds the death and the damage, not the far cash move");
		ctx.Equal(1, LF_IncidentLog.Query(Vector(100, 0, 100), 10, 1005, -1, "").Count(), "a window starting after the death drops it");
		ctx.Equal(1, LF_IncidentLog.Query(Vector(100, 0, 100), -1, 0, 1500, LF_IncidentConfig.KIND_DEATH).Count(), "kind and end of window filter");
		ctx.Equal(3, LF_IncidentLog.Query(vector.Zero, -1, 0, -1, "").Count(), "a negative radius ignores position");
		if (near.Count() == 2)
		{
			ctx.True(near[0].m_bMurder, "the death keeps its murder flag");
			ctx.EqualStr("rifle", near[0].m_sHow, "the death keeps the weapon");
			ctx.EqualStr("killer-a", near[0].m_sOtherUid, "the death keeps the killer");
		}

		// Bound: count first, then retention.
		array<ref LF_IncidentRecord> list = new array<ref LF_IncidentRecord>();
		for (int i = 0; i < 5; i++)
		{
			LF_IncidentRecord record = new LF_IncidentRecord();
			record.m_iNumber = i + 1;
			record.m_iTime = 100 + i;
			list.Insert(record);
		}

		ctx.Equal(2, LF_IncidentLog.Trim(list, 104, 1000, 3), "five records over a cap of three drop two");
		ctx.Equal(3, list.Count(), "three remain");
		ctx.Equal(3, list[0].m_iNumber, "the oldest went first");
		ctx.Equal(1, LF_IncidentLog.Trim(list, 110, 7, 10), "a record older than the retention goes");
		ctx.Equal(2, list.Count(), "two remain after retention");

		// Kinds from config.
		ctx.True(LF_IncidentConfig.KindEnabled("all", "damage"), "all records every kind");
		ctx.True(LF_IncidentConfig.KindEnabled("Death, cash", "cash"), "a listed kind is recorded");
		ctx.False(LF_IncidentConfig.KindEnabled("death,cash", "damage"), "an unlisted kind is not");
		ctx.False(LF_IncidentConfig.KindEnabled("", "death"), "an empty list records nothing");

		// Damage sampling: a burst of hits is one record, the next window another.
		LF_IncidentDamage.ResetStatics();
		ctx.EqualFloat(10, LF_IncidentDamage.Offer("1>2", 10, 100, 5), 0.01, "the first hit leaves at once");
		ctx.EqualFloat(0, LF_IncidentDamage.Offer("1>2", 10, 101, 5), 0.01, "a hit inside the window is held");
		ctx.EqualFloat(0, LF_IncidentDamage.Offer("1>2", 10, 104, 5), 0.01, "so is the next");
		ctx.EqualFloat(30, LF_IncidentDamage.Offer("1>2", 10, 106, 5), 0.01, "the next window carries what gathered");
		ctx.EqualFloat(10, LF_IncidentDamage.Offer("2>1", 10, 106, 5), 0.01, "another pair has its own window");
		ctx.EqualFloat(0, LF_IncidentDamage.Offer("2>1", 0, 200, 5), 0.01, "no damage is no record");

		// The bus fills the log through the real subscribers.
		LF_IncidentLog.ResetStatics();
		LF_IncidentLog.Install();
		LF_OnPlayerKilled killed = LF_OnPlayerKilled.Create("victim-b", "killer-b");
		killed.m_vPosition = Vector(10, 0, 10);
		killed.m_sWeapon = "pistol";
		killed.m_bMurder = true;
		LF_EventBus.Publish(killed);
		LF_EventBus.Publish(LF_OnPlayerDamaged.Create("victim-b", "killer-b", 41.6, Vector(10, 0, 10), "pistol"));
		LF_EventBus.Publish(LF_OnMoneyMoved.Create("payer-b", 75, "atm-withdraw"));
		ctx.Equal(3, LF_IncidentLog.Count(), "a death, a damage sample, and a cash move reached the log through the bus");
		array<ref LF_IncidentRecord> all = LF_IncidentLog.Query(vector.Zero, -1, 0, -1, "");
		if (all.Count() == 3)
		{
			ctx.EqualStr(LF_IncidentConfig.KIND_DEATH, all[0].m_sKind, "the first is the death");
			ctx.True(all[0].m_bMurder, "the bus carried the murder flag");
			ctx.Equal(42, all[1].m_iAmount, "damage is rounded");
			ctx.EqualStr("atm-withdraw", all[2].m_sHow, "cash keeps its source");
		}

		LF_IncidentLog.Install();
		LF_EventBus.Publish(LF_OnMoneyMoved.Create("payer-b", 1, "atm-withdraw"));
		ctx.Equal(4, LF_IncidentLog.Count(), "a second Install does not double-subscribe");

		LF_IncidentLog.ResetStatics();
		LF_IncidentLog.ApplyAll(before);
	}
}
