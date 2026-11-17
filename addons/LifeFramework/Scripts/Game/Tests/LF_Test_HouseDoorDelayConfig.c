// red-proof: Observed 2026-11-17: `tools\cli test --no-build --red LF_Test_HouseDoorDelayConfig` printed RED-OK (with the fixture skipped: "doorCloseDelaySeconds 5 gives a 5 s delay (expected 5, got 20)"; no houses key is read on main).
// tier: WORLD
//! The `houses` owner section (#2505, slice of #2435): `doorCloseDelaySeconds` is how long a locked
//! house's open door stays open before LF_HouseDoorClose closes it. 5 gives a 5 s delay, 0 turns the
//! auto-close off, 9999 clamps to 600 (with a warning), a missing key keeps the shipped 20, and a
//! negative or non-numeric value keeps the shipped 20.
class LF_Test_HouseDoorDelayConfig : LF_Test
{
	override string GetName()
	{
		return "houses/door-delay-config";
	}

	protected float DelayFor(string json, string source)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString(json, source);
		LF_HousesConfig.Reload();
		return LF_HouseDoorClose.GetDelaySeconds();
	}

	override void Run(LF_TestContext ctx)
	{
		// No section, or a section without the key: the shipped 20 seconds.
		ctx.EqualFloat(20, DelayFor("{\"schemaVersion\":1}", "test:houses-none"), 0.001, "no houses section keeps the shipped 20 s delay");
		ctx.EqualFloat(20, DelayFor("{\"schemaVersion\":1,\"houses\":{}}", "test:houses-empty"), 0.001, "a houses section without the key keeps 20 s");
		ctx.True(LF_ConfigService.IsKnownTopLevelKey(LF_HousesConfig.SECTION), "houses is a known top-level key");

		// The owner's value.
		string five = "{\"schemaVersion\":1,\"houses\":{\"doorCloseDelaySeconds\":5}}";
		if (RedProof())
			five = "{\"schemaVersion\":1}";

		ctx.EqualFloat(5, DelayFor(five, "test:houses-five"), 0.001, "doorCloseDelaySeconds 5 gives a 5 s delay");
		ctx.EqualFloat(0.5, DelayFor("{\"schemaVersion\":1,\"houses\":{\"doorCloseDelaySeconds\":0.5}}", "test:houses-half"), 0.001, "a fractional delay is kept");

		// 0 turns the auto-close off: no timer is armed and the delay reads 0.
		ctx.EqualFloat(0, DelayFor("{\"schemaVersion\":1,\"houses\":{\"doorCloseDelaySeconds\":0}}", "test:houses-off"), 0.001, "doorCloseDelaySeconds 0 turns the auto-close off");

		// Over the limit: clamped to 600 (ApplyOwner logs the warning).
		ctx.EqualFloat(600, DelayFor("{\"schemaVersion\":1,\"houses\":{\"doorCloseDelaySeconds\":9999}}", "test:houses-huge"), 0.001, "doorCloseDelaySeconds 9999 clamps to 600");
		ctx.EqualFloat(600, DelayFor("{\"schemaVersion\":1,\"houses\":{\"doorCloseDelaySeconds\":600}}", "test:houses-max"), 0.001, "the limit itself is accepted");

		// A bad value keeps the shipped one.
		ctx.EqualFloat(20, DelayFor("{\"schemaVersion\":1,\"houses\":{\"doorCloseDelaySeconds\":-3}}", "test:houses-negative"), 0.001, "a negative delay keeps 20 s");
		ctx.EqualFloat(20, DelayFor("{\"schemaVersion\":1,\"houses\":{\"doorCloseDelaySeconds\":\"soon\"}}", "test:houses-text"), 0.001, "a non-numeric delay keeps 20 s");

		// A reload applies the new owner file.
		DelayFor("{\"schemaVersion\":1,\"houses\":{\"doorCloseDelaySeconds\":7}}", "test:houses-seven");
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"houses\":{\"doorCloseDelaySeconds\":9}}", "test:houses-nine");
		ctx.EqualFloat(7, LF_HouseDoorClose.GetDelaySeconds(), 0.001, "the cached delay holds until a reload");
		LF_HousesConfig.Reload();
		ctx.EqualFloat(9, LF_HouseDoorClose.GetDelaySeconds(), 0.001, "a config reload applies the new delay");

		LF_ConfigService.Reset();
		LF_HousesConfig.Reload();
	}
}
