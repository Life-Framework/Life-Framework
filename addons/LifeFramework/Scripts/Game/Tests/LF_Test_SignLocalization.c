// red-proof: expected "Read: Bank!"; `tools\cli test --no-build --tier all`
// (2026-11-08) failed signs/localization with "a literal title reads
// Read: <title> (expected 'Read: Bank!', got 'Read: Bank')". The same run
// showed the dedicated test server resolves sign keys.

// tier: LOGIC
class LF_Test_SignLocalization : LF_Test
{
	override string GetName()
	{
		return "signs/localization";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_SignCatalog.IsKey("#LF-Sign_What"), "a # value is a key");
		ctx.False(LF_SignCatalog.IsKey("Bank"), "a plain value is not a key");
		ctx.False(LF_SignCatalog.IsKey("#"), "a lone # is not a key");
		ctx.False(LF_SignCatalog.IsKey(""), "an empty value is not a key");

		ctx.EqualStr("Bring cash, #1 rule: no guns.", LF_SignCatalog.Resolve("Bring cash, #1 rule: no guns."), "a literal is shown as written");
		ctx.EqualStr("What it is", LF_SignCatalog.Resolve("#LF-Sign_What"), "a key resolves through the string table");
		// The per-sign text is the pack's (#325): with the pack loaded the key resolves, without it the card
		// falls back to the core's neutral title and skips the sections it has no text for.
		string unknownKey = "#" + "LF-Sign_Nope_Title"; // a key no table declares
		bool packText = WidgetManager.Translate("#LF-Sign_Mining_Title") != "#LF-Sign_Mining_Title";
		ctx.EqualStr("", LF_SignCatalog.Resolve(unknownKey), "a key no table knows resolves to nothing");
		ctx.EqualStr("Notice", LF_SignCatalog.ResolveTitle(unknownKey), "a title no table knows reads the neutral title");
		ctx.EqualStr("Read: Notice", LF_SignCatalog.ReadName(LF_Test_SignCatalog.MakeSign("nope", unknownKey, "", "", "", "")), "a sign with no text prompts Read: Notice");
		if (packText)
			ctx.EqualStr("Mining area", LF_SignCatalog.Resolve("#LF-Sign_Mining_Title"), "a pack sign title key resolves");

		LF_SignDef literal = LF_Test_SignCatalog.MakeSign("bank", "Bank", "", "", "", "");
		ctx.EqualStr("Read: Bank", LF_SignCatalog.ReadName(literal), "a literal title reads Read: <title>");

		LF_SignDef keyed = LF_Test_SignCatalog.MakeSign("mining", "#LF-Sign_Mining_Title", "#LF-Sign_Mining_What", "", "", "");
		if (packText)
		{
			ctx.EqualStr("Read: Mining area", LF_SignCatalog.ReadName(keyed), "a keyed title reads Read: <translated title>");
			ctx.EqualStr("MINING AREA", LF_SignComponent.FaceText(keyed), "the board face prints the translated title in capitals");
		}
		else
		{
			ctx.EqualStr("Read: Notice", LF_SignCatalog.ReadName(keyed), "without the pack a keyed title reads the neutral title");
		}
		ctx.EqualStr("BANK", LF_SignComponent.FaceText(literal), "the board face prints a literal title in capitals");
		ctx.False(LF_SignComponent.WantsFace(true, true, true), "a dedicated server draws no board face");
		ctx.False(LF_SignComponent.WantsFace(true, false, false), "no workspace, no board face");
		ctx.False(LF_SignComponent.WantsFace(false, false, true), "a sign with the face off draws none");
		ctx.True(LF_SignComponent.WantsFace(true, false, true), "a rendering client draws the board face");

		array<ref LF_UIPart> parts = LF_SignCatalog.BuildParts(keyed);
		if (packText)
		{
			ctx.Equal(5, parts.Count(), "a keyed sign builds title, rule, What (label and body), and Close");
			if (parts.Count() != 5)
				return;

			ctx.EqualStr("Mining area", parts.Get(0).GetText(), "the card title is translated");
			ctx.EqualStr("WHAT IT IS", parts.Get(2).GetText(), "the section heading is translated");
			ctx.False(parts.Get(3).GetText().Contains("#LF-"), "no raw key reaches the card");
		}
		else
		{
			ctx.Equal(3, parts.Count(), "a sign with no text builds title, rule and Close");
			ctx.EqualStr("Notice", parts.Get(0).GetText(), "the card title is the neutral one");
		}
	}
}
