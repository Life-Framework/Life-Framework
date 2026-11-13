// red-proof: observed 2026-11-13 with `tools\cli test --no-build --only LF_Test_Credits,LF_Test_CreditsWorld`: with the
// missing-key assertion flipped to True it printed `[LFTEST] FAIL signs/credits` / `- a missing key is not printed on
// the card`; restored, green.
// tier: LOGIC
//! Credits notice (#481): the card lists every row of the credits source, and a key the string
//! table lacks is left out instead of printed.
class LF_Test_Credits : LF_Test
{
	override string GetName()
	{
		return "signs/credits";
	}

	//------------------------------------------------------------------------------------------------
	protected string Bodies(array<ref LF_UIPart> parts)
	{
		string all;
		foreach (LF_UIPart part : parts)
		{
			if (part.GetKind() == LF_EUIPartKind.RICH)
				all += part.GetText() + "\n";
		}

		return all;
	}

	//------------------------------------------------------------------------------------------------
	protected int CountKind(array<ref LF_UIPart> parts, LF_EUIPartKind kind)
	{
		int count;
		foreach (LF_UIPart part : parts)
		{
			if (part.GetKind() == kind)
				count = count + 1;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ref array<string> kinds = {};
		ref array<string> keys = {};
		LF_CreditsRows.Fill(kinds, keys);
		ctx.True(kinds.Count() == LF_CreditsRows.COUNT && keys.Count() == kinds.Count(), "the generated list has one key per row");
		ctx.True(kinds.Count() >= 8, "the credits source yields rows");
		ctx.EqualStr("H", kinds[0], "the list starts with a section heading");

		// Built by hand so the localization check does not read it as a key that should exist.
		string bare = "LF-Credits" + "_R_NOSUCHKEY";
		string fake = "#" + bare;
		ctx.True(LF_Credits.IsMissing(fake, fake), "a key handed back unchanged is missing");
		ctx.True(LF_Credits.IsMissing(fake, bare), "a key handed back without its hash sign is missing");
		ctx.True(LF_Credits.IsMissing(fake, ""), "an empty text is missing");
		ctx.False(LF_Credits.IsMissing(fake, "Piper"), "a real text is not missing");

		ref LF_SignDef sign = new LF_SignDef();
		sign.m_sId = LF_Credits.SIGN_ID;
		sign.m_sTitle = "#LF-Credits_Title";
		ctx.True(LF_Credits.IsCreditsSign(sign), "the credits sign is recognised by its id");
		ctx.False(LF_Credits.IsCreditsSign(null), "no sign is not the credits sign");

		array<ref LF_UIPart> parts = LF_Credits.BuildParts(sign);
		ctx.True(parts.Count() > 4, "the card has parts");
		ctx.True(parts[0].GetKind() == LF_EUIPartKind.TITLE, "the card starts with its title");
		ctx.EqualStr("Credits", parts[0].GetText(), "the title key resolves");
		ctx.True(parts[parts.Count() - 1].GetKind() == LF_EUIPartKind.CLOSE, "the card ends with Close");

		int headings;
		int absent;
		string bodies = Bodies(parts);
		foreach (int i, string key : keys)
		{
			string text = LF_SignCatalog.Resolve(key);
			if (LF_Credits.IsMissing(key, text))
			{
				absent = absent + 1;
				continue;
			}

			if (kinds[i] == "H")
				headings = headings + 1;
			else if (!bodies.Contains(text))
				ctx.True(false, "the card lists the credit '" + text + "'");
		}

		ctx.True(absent == 0, "every generated credit key has text in the string table");
		ctx.True(CountKind(parts, LF_EUIPartKind.HEADING) == headings, "the card has one label per section of the source");
		ctx.True(bodies.Contains(LF_SignCatalog.Resolve(LF_Credits.KEY_MORE)), "the card ends by pointing at the full list");

		// A key the string table lacks: that line is left out, its neighbours stay.
		ref array<string> brokenKinds = {"H", "R", "R"};
		ref array<string> brokenKeys = {keys[0], fake, keys[1]};
		string broken = Bodies(LF_Credits.BuildParts(sign, brokenKinds, brokenKeys));
		ctx.False(broken.Contains("NOSUCHKEY"), "a missing key is not printed on the card");
		ctx.True(broken.Contains(LF_SignCatalog.Resolve(keys[1])), "the line after a missing key still shows");

		ctx.True(LF_Credits.BuildParts(null).Count() == 0, "no sign builds no card");
	}
}
