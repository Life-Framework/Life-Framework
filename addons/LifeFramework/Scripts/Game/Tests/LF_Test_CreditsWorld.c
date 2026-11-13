// red-proof: observed 2026-11-13 in the same run: with the last assertion flipped to False it printed
// `[LFTEST] FAIL signs/credits-world` / `- the card credits the project Life Framework continues`; restored, green.
// tier: WORLD
//! Credits notice (#481): the sign catalog carries the credits entry and its card is the credits.
class LF_Test_CreditsWorld : LF_Test
{
	override string GetName()
	{
		return "signs/credits-world";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_SignDef sign = LF_SignCatalog.Find(LF_Credits.SIGN_ID);
		ctx.True(sign != null, "LF_Signs.conf has the credits entry");
		if (!sign)
			return;

		ctx.True(LF_SignCatalog.IsUsable(sign), "the credits entry has a title, so Read is offered");
		ctx.True(LF_Credits.IsCreditsSign(sign), "the entry is the one the card switches on");
		ctx.EqualStr("Read: Credits", LF_SignCatalog.ReadName(sign), "the F-key prompt names the notice");

		array<ref LF_UIPart> card = LF_Credits.BuildParts(sign);
		array<ref LF_UIPart> plain = LF_SignCatalog.BuildParts(sign);
		ctx.True(card.Count() > plain.Count(), "the credits card carries more than the plain sign card");

		bool everon;
		foreach (LF_UIPart part : card)
		{
			if (part.GetKind() == LF_EUIPartKind.RICH && part.GetText().Contains("Everon Life RPG Framework"))
				everon = true;
		}

		ctx.True(everon, "the card credits the project Life Framework continues");
	}
}
