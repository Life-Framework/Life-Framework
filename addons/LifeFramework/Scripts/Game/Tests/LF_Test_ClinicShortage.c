// red-proof: 2026-11-23 `tools/cli test --no-build --red LF_Test_ClinicShortage` printed `RED PROOF OK`; RedProof() skips the cabinet breach (`the shortage lasts the configured time (expected 600, got 0)`, `the counter's medkit costs the config percent (expected 200, got 100)`). Green without the hook.
//! Clinic shortage (#4445): a breach that opens a hospital cabinet makes the clinic short. While short
//! the counter's buy price is raised by the config percent (an on-duty medic restocks at the normal
//! price) and a revive at the clinic takes longer; a second robbery restarts the clock without
//! stacking; after the duration both are back to normal. A breach of another target starts nothing.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ClinicShortage : LF_Test
{
	static const string CABINET_ID = "clinic-shortage-test-cabinet";
	static const string DOOR_ID = "clinic-shortage-test-door";
	static const string OUTLET = "clinic_shortage_test_counter";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "crime/clinic-shortage";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ClinicShortage.ResetStatics();
		LF_ClinicShortageConfig config = LF_ClinicShortageConfig.Defaults();
		config.m_iDurationSeconds = 600;
		config.m_iPricePercent = 200;
		config.m_iReviveTimePercent = 150;
		config.m_aOutlets = {OUTLET};
		LF_ClinicShortageConfig.UseForTest(config);
		LF_ClinicShortage.s_iNowOverride = 1000;

		array<ResourceName> loot = {};
		LF_Hospital.FillDefaultLoot(loot);
		LF_Hospital.ConfigureCabinet(CABINET_ID, loot);
		ctx.True(LF_ClinicShortage.Install(), "the shortage listens on the bus");

		// A breach of something that is not a hospital cabinet starts nothing.
		LF_EventBus.Publish(LF_OnBreachOpened.Create(DOOR_ID, "clinic-shortage-robber"));
		ctx.False(LF_ClinicShortage.IsActive(), "a non-hospital breach starts no shortage");
		ctx.Equal(100, LF_ClinicShortage.UnitPrice(OUTLET, 100, false), "the counter's price is normal");
		ctx.True(LF_ClinicShortage.ReviveSlice(1.0, true) == 1.0, "the clinic revive time is normal");

		// The cabinet is robbed: the clinic is short.
		if (!RedProof())
			LF_EventBus.Publish(LF_OnBreachOpened.Create(CABINET_ID, "clinic-shortage-robber"));

		ctx.True(LF_ClinicShortage.IsActive(), "a hospital cabinet breach starts the shortage");
		ctx.Equal(600, LF_ClinicShortage.SecondsLeft(), "the shortage lasts the configured time");
		ctx.Equal(200, LF_ClinicShortage.UnitPrice(OUTLET, 100, false), "the counter's medkit costs the config percent");
		ctx.Equal(100, LF_ClinicShortage.UnitPrice(OUTLET, 100, true), "an on-duty medic restocks at the normal price");
		ctx.Equal(100, LF_ClinicShortage.UnitPrice("clinic_shortage_other_outlet", 100, false), "another outlet's price is untouched");
		float slice = LF_ClinicShortage.ReviveSlice(1.0, true);
		ctx.True(slice < 0.67 && slice > 0.66, string.Format("a revive at the clinic takes one and a half times as long (slice %1)", slice));
		ctx.True(LF_ClinicShortage.ReviveSlice(1.0, false) == 1.0, "a revive away from the clinic is not slowed");

		// A second robbery restarts the clock; it never stacks a second hour.
		LF_ClinicShortage.s_iNowOverride = 1300;
		LF_EventBus.Publish(LF_OnBreachOpened.Create(CABINET_ID, "clinic-shortage-robber"));
		ctx.Equal(600, LF_ClinicShortage.SecondsLeft(), "a robbery while short restarts at the full duration, no stacking");

		// The time runs out: back to normal on its own.
		LF_ClinicShortage.s_iNowOverride = 1900;
		LF_ClinicShortage.Expire();
		ctx.False(LF_ClinicShortage.IsActive(), "after the duration the clinic is stocked again");
		ctx.Equal(0, LF_ClinicShortage.SecondsLeft(), "no seconds left after the shortage");
		ctx.Equal(100, LF_ClinicShortage.UnitPrice(OUTLET, 100, false), "the counter's price is normal again");
		ctx.True(LF_ClinicShortage.ReviveSlice(1.0, true) == 1.0, "the clinic revive time is normal again");

		// Leave the live listener installed for the rest of the session.
		LF_ClinicShortage.ResetStatics();
		LF_ClinicShortage.Install();
	}
}
