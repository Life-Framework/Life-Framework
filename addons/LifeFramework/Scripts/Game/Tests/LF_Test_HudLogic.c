// red-proof: 2026-11-22 (needs icons, brand over the map added): `--red` fails the brand-on-map check too; earlier observed 2026-11-22 `tools\cli test --no-build --red LF_Test_HudLogic` printed `RED-OK hud/subtle-rules` (RedProof() skips `/hud money off`, so "/hud money off hides the money" and "the saved line keeps money off" fail); `--only` then passed=1 failed=0. The needs, money, delta, bank and wanted rules are pure and pinned in the same run.
//! The subtle HUD's rules (LF_HudLogic, LF_HudPrefs): needs hide while both are high and show when
//! one drops or after eating, low reads red, the money block dims after idle unless forced, the
//! delta and bank text, wanted pips clamp to 5 and pulse only on a rise, and the /hud command.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_HudLogic : LF_Test
{
	override string GetName()
	{
		return "hud/subtle-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		// Needs: hidden while both comfortable, shown when one drops or just after a rise.
		ctx.False(LF_HudLogic.NeedsVisible(80, 90, -1), "both needs high: hidden");
		ctx.True(LF_HudLogic.NeedsVisible(40, 90, -1), "food below half: shown");
		ctx.True(LF_HudLogic.NeedsVisible(80, 10, -1), "drink low: shown");
		ctx.True(LF_HudLogic.NeedsVisible(80, 90, 1.0), "just ate: shown for a moment");
		ctx.False(LF_HudLogic.NeedsVisible(80, 90, 10.0), "long after eating: hidden again");
		ctx.True(LF_HudLogic.NeedLow(10), "10 is low");
		ctx.False(LF_HudLogic.NeedLow(60), "60 is not low");
		ctx.EqualStr("100", LF_HudLogic.NeedText(130), "a need prints clamped to 100");

		// Needs icons (first-2-hours review C15) and the brand mark over the map (C6).
		float lowOpacity = 1;
		float calmOpacity = 1;
		bool brandOnMap = true;
		if (!RedProof())
		{
			lowOpacity = LF_HudLogic.NeedOpacity(30);
			calmOpacity = LF_HudLogic.NeedOpacity(80);
			brandOnMap = LF_HudLogic.BrandVisible(true, true);
		}

		ctx.True(lowOpacity > 0.99, "a need under half draws at full strength");
		ctx.True(calmOpacity < 0.5, "a need still above half fades back beside the one that dropped");
		ctx.False(brandOnMap, "the brand mark is off while the map is open");
		ctx.True(LF_HudLogic.BrandVisible(true, false), "and on the rest of the time");
		ctx.False(LF_HudLogic.BrandVisible(false, false), "unless the player turned it off");

		// Money block: full while fresh or forced, dim after idle.
		ctx.True(LF_HudLogic.MoneyOpacity(1, false) > 0.9, "fresh change: full brightness");
		ctx.True(LF_HudLogic.MoneyOpacity(30, false) < 0.5, "idle: dimmed");
		ctx.True(LF_HudLogic.MoneyOpacity(30, true) > 0.9, "inventory or map open: full brightness");

		// Delta and bank text.
		ctx.EqualStr("+" + LF_MoneyFormat.FormatMoney(100), LF_HudLogic.DeltaText(1140, 1240), "a gain reads +$100");
		ctx.EqualStr(LF_MoneyFormat.FormatMoney(-18), LF_HudLogic.DeltaText(1240, 1222), "a spend reads -$18");
		ctx.EqualStr("", LF_HudLogic.DeltaText(5, 5), "no change, no delta");
		ctx.EqualStr("", LF_HudLogic.BankAmountText(LF_HudLogic.BANK_UNKNOWN), "unknown balance prints nothing");
		ctx.EqualStr(LF_MoneyFormat.FormatMoney(-150), LF_HudLogic.BankAmountText(-150), "a hospital debt prints as -$150, not blank");
		ctx.EqualStr(LF_MoneyFormat.FormatMoney(18900), LF_HudLogic.BankAmountText(18900), "bank prints as money");

		// Wanted.
		ctx.Equal(2, LF_HudLogic.WantedPips(2), "two pips at level 2");
		ctx.Equal(5, LF_HudLogic.WantedPips(9), "pips clamp to five");
		ctx.Equal(0, LF_HudLogic.WantedPips(-3), "no negative pips");
		ctx.True(LF_HudLogic.WantedRose(1, 2), "1 to 2 pulses");
		ctx.False(LF_HudLogic.WantedRose(2, 0), "clearing does not pulse");

		// /hud settings.
		LF_HudPrefs prefs = new LF_HudPrefs();
		bool changed;
		if (!RedProof())
			prefs.Apply("money off", changed);
		ctx.False(prefs.IsOn(LF_HudPrefs.PART_MONEY), "/hud money off hides the money");
		ctx.True(prefs.IsOn(LF_HudPrefs.PART_WANTED), "other parts stay on");
		ctx.EqualStr("#LF-Hud_CommandDone", prefs.Apply("  WANTED Off ", changed), "case and spaces are forgiven");
		ctx.True(changed, "a real change reports changed");
		ctx.EqualStr("#LF-Hud_CommandUsage", prefs.Apply("bogus on", changed), "an unknown part gets the usage line");
		ctx.EqualStr("#LF-Hud_CommandUsage", prefs.Apply("needs maybe", changed), "a bad state gets the usage line");
		string line = prefs.ToLine();
		LF_HudPrefs loaded = new LF_HudPrefs();
		loaded.FromLine(line);
		ctx.False(loaded.IsOn(LF_HudPrefs.PART_MONEY), "the saved line keeps money off");
		ctx.True(loaded.IsOn(LF_HudPrefs.PART_BRAND), "the saved line keeps brand on");
		prefs.Apply("all on", changed);
		ctx.True(prefs.IsOn(LF_HudPrefs.PART_MONEY) && prefs.IsOn(LF_HudPrefs.PART_WANTED), "/hud all on restores everything");
	}
}
