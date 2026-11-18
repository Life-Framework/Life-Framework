// red-proof: observed 2026-11-18 `tools/cli test --no-build --red LF_Test_BarberHostAndPortrait`: RED-OK; with EndPreview skipped `a listen host pays for the look it previewed (expected 0, got 3)` (SAME_LOOK) and `the fee is taken once (expected 60, got 100)`. Green with `--only` (12 assertions).
//! Review of #3234. A listen host's preview runs on the server: Pay must put the old head back first, or
//! the server takes the previewed look for the one worn and refuses it (SAME_LOOK). The haircut keeps the
//! worn clothes, and the look a wanted poster or photo ID captures carries the new head.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_BarberHostAndPortrait : LF_Test
{
	static const string KEY = "test-barber-host";
	static const int FEE = 40;

	override string GetName()
	{
		return "barber/host-and-portrait";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		LF_Barber.Forget(KEY);
		ResourceName before = LF_Barber.CurrentHead(character);
		int look = LF_BarberSheet.Step(LF_Barber.IndexOf(before), 1, LF_Barber.Looks().Count());
		ResourceName wanted = LF_Barber.LookAt(look);
		array<ResourceName> wornBefore = {};
		LF_WantedLook.WornPrefabs(character, wornBefore);
		ctx.True(wornBefore.Count() >= 1, "the fixture wears clothes");

		// The listen host previews the look on the server, then pays.
		LF_Barber.ApplyHead(character, wanted);
		if (!RedProof())
			LF_BarberSheet.EndPreview(character, before);

		LF_MoneyUtils.AddCash(character, 100);
		int cash = LF_MoneyUtils.GetCash(character);
		ctx.Equal(LF_EBarberRefusal.NONE, LF_Barber.Cut(character, KEY, look, FEE), "a listen host pays for the look it previewed");
		ctx.Equal(cash - FEE, LF_MoneyUtils.GetCash(character), "the fee is taken once");
		ctx.True(LF_Barber.CurrentHead(character) == wanted, "the host wears the paid look");

		array<ResourceName> wornAfter = {};
		LF_WantedLook.WornPrefabs(character, wornAfter);
		ctx.Equal(wornBefore.Count(), wornAfter.Count(), "the haircut keeps every worn item");
		bool same = true;
		foreach (ResourceName worn : wornBefore)
		{
			if (wornAfter.Find(worn) < 0)
				same = false;
		}

		ctx.True(same, "the same items are worn after the haircut");

		string captured = LF_WantedLook.Capture(character);
		ctx.True(LF_WantedLook.HeadOf(captured) == "{" + LF_WantedLook.GuidOf(wanted) + "}", "the captured look carries the new head: " + captured);
		ResourceName prefab;
		array<int> slots = {};
		array<ResourceName> items = {};
		ctx.True(LF_WantedLook.Parse(captured, prefab, slots, items), "the look with a head still parses");
		ctx.Equal(wornAfter.Count(), items.Count(), "the head part is not read as a worn item");
		ctx.True(LF_WantedLook.HeadOf("9B5BB216CC7FF18E|2=9F546CCA2582D16F").IsEmpty(), "a look from before heads has no head");

		LF_Barber.Forget(KEY);
		LF_TestCharacterFixture.Delete(character);
	}
}
