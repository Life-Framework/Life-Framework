// red-proof: Observed 2026-11-13 with `tools\cli test --no-build --only LF_Test_PhotoIdCard,LF_Test_PhotoIdCardWorld`:
// made LF_PhotoIdCard.IsValid return true for any known number (no void check); got
// `[LFTEST] FAIL identity/photo-id-card`, `- the replaced card is void`,
// `- the replaced card is still void (apply 1)`. Restored; green.
// tier: LOGIC
//! Photo identity card (#902), with no world: the registry numbers cards in order, the newest card of
//! a holder is valid and an older one void, the rows survive an export and apply (idempotent, the
//! number keeps counting), the clerk's and the spawn's rules, the police request's window, and the
//! card sheet's parts.
class LF_Test_PhotoIdCard : LF_Test
{
	override string GetName()
	{
		return "identity/photo-id-card";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_PhotoIdRow> kept = LF_PhotoIdCard.ExportAll();
		LF_PhotoIdCard.ResetStatics();
		CheckRegistry(ctx);
		CheckRules(ctx);
		CheckSheet(ctx);
		LF_PhotoIdCard.ApplyAll(kept);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRegistry(LF_TestContext ctx)
	{
		ctx.EqualStr("EV 000042", LF_PhotoIdCard.FormatNumber(42), "a card number is the prefix and six digits");
		ctx.Equal(42, LF_PhotoIdCard.SequenceOf("EV 000042"), "the running number reads back");
		ctx.Equal(0, LF_PhotoIdCard.SequenceOf("EV 42"), "a short number is not a card number");
		ctx.Equal(0, LF_PhotoIdCard.SequenceOf("DRILL-0001"), "a fence serial is not a card number");
		ctx.True(LF_PhotoIdCard.Issue("", "No", "One", 20, "01/06/1989", "") == null, "a card with no holder is not recorded");

		LF_PhotoIdRow first = LF_PhotoIdCard.Issue("holder-a", "Ada", "Novak", 31, "01/06/1989", "9B5BB216CC7FF18E");
		LF_PhotoIdRow other = LF_PhotoIdCard.Issue("holder-b", "Bob", "Kral", 45, "02/06/1989", "");
		ctx.EqualStr("EV 000001", first.m_sNumber, "the first card is number 1");
		ctx.EqualStr("EV 000002", other.m_sNumber, "the next card is number 2");
		ctx.True(LF_PhotoIdCard.Current("holder-a") == first, "a holder's only card is their current one");
		ctx.True(LF_PhotoIdCard.IsValid(first.m_sNumber), "a holder's only card is valid");

		LF_PhotoIdRow replacement = LF_PhotoIdCard.Issue("holder-a", "Ada", "Novak", 31, "09/06/1989", "9B5BB216CC7FF18E|1=9F546CCA2582D16F");
		ctx.EqualStr("EV 000003", replacement.m_sNumber, "a replacement gets a new number");
		ctx.True(LF_PhotoIdCard.Current("holder-a") == replacement, "the replacement is the holder's current card");
		ctx.False(LF_PhotoIdCard.IsValid(first.m_sNumber), "the replaced card is void");
		ctx.True(LF_PhotoIdCard.IsValid(other.m_sNumber), "another holder's card stays valid");
		ctx.True(LF_PhotoIdCard.FindNumber(first.m_sNumber) == first, "a void card still reads back by its number");
		ctx.False(LF_PhotoIdCard.IsValid("EV 000099"), "an unknown number is not valid");

		// The "idcards" payload: export, wipe, apply twice; the same registry and the next number.
		array<ref LF_PhotoIdRow> rows = LF_PhotoIdCard.ExportAll();
		rows.Insert(LF_PhotoIdRow.Create("junk", "holder-x", "X", "Y", 1, "", ""));
		rows.Insert(LF_PhotoIdRow.Create("EV 000007", "", "X", "Y", 1, "", ""));
		for (int pass = 1; pass <= 2; pass++)
		{
			LF_PhotoIdCard.ResetStatics();
			string tag = string.Format(" (apply %1)", pass);
			ctx.Equal(3, LF_PhotoIdCard.ApplyAll(rows), "the three cards apply; a bad number or no holder is skipped" + tag);
			ctx.EqualStr("EV 000003", LF_PhotoIdCard.Current("holder-a").m_sNumber, "the replacement is still current" + tag);
			ctx.False(LF_PhotoIdCard.IsValid("EV 000001"), "the replaced card is still void" + tag);
			ctx.EqualStr("9B5BB216CC7FF18E|1=9F546CCA2582D16F", LF_PhotoIdCard.Current("holder-a").m_sLook, "the photo comes back" + tag);
			ctx.EqualStr("09/06/1989", LF_PhotoIdCard.Current("holder-a").m_sIssued, "the date of issue comes back" + tag);
		}

		LF_PhotoIdRow next = LF_PhotoIdCard.Issue("holder-c", "Cyril", "Dupont", 52, "10/06/1989", "");
		ctx.EqualStr("EV 000004", next.m_sNumber, "numbering continues after the highest loaded number");
		ctx.Equal(0, LF_PhotoIdCard.ApplyAll(null), "no rows apply nothing");
		ctx.Equal(0, LF_PhotoIdCard.Count(), "and leave an empty registry");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRules(LF_TestContext ctx)
	{
		ctx.Equal(LF_EIdReplace.NEVER_ISSUED, LF_PhotoIdCard.CheckReplace(false, false, 500, 50), "never issued: the arrival chain hands out the first card");
		ctx.Equal(LF_EIdReplace.STILL_CARRIED, LF_PhotoIdCard.CheckReplace(true, true, 500, 50), "a player carrying their valid card gets no second one");
		ctx.Equal(LF_EIdReplace.NOT_ENOUGH_CASH, LF_PhotoIdCard.CheckReplace(true, false, 49, 50), "the fee is paid in full");
		ctx.Equal(LF_EIdReplace.OK, LF_PhotoIdCard.CheckReplace(true, false, 50, 50), "a lost card is replaced for the fee");
		ctx.Equal(LF_EIdReplace.OK, LF_PhotoIdCard.CheckReplace(true, false, 0, 0), "a free replacement needs no cash");

		ctx.Equal(LF_EIdSpawn.NOTHING, LF_PhotoIdCard.DecideSpawn(false, true, true), "a spawn with the valid card in the pocket changes nothing");
		ctx.Equal(LF_EIdSpawn.NOTHING, LF_PhotoIdCard.DecideSpawn(false, true, false), "a lost card stays lost: the clerk replaces it");
		ctx.Equal(LF_EIdSpawn.FIRST_CARD, LF_PhotoIdCard.DecideSpawn(false, false, false), "a resident who earned a card before the registry gets one");
		ctx.Equal(LF_EIdSpawn.FREE_REPLACEMENT, LF_PhotoIdCard.DecideSpawn(true, true, false), "a mode with no clerk replaces a lost card at the next spawn");
		ctx.Equal(LF_EIdSpawn.NOTHING, LF_PhotoIdCard.DecideSpawn(true, true, true), "and gives no second one");

		ctx.True(LF_IdCheck.IsOpen(100, 100 + LF_IdCheck.REQUEST_SECONDS), "a request is open for its whole window");
		ctx.False(LF_IdCheck.IsOpen(100, 101 + LF_IdCheck.REQUEST_SECONDS), "an unanswered request lapses");
		ctx.False(LF_IdCheck.IsOpen(-1, 5), "no request, nothing open");

		ctx.EqualStr("#LF-IdCard_ClerkReplaced", LF_IdCardClerk.AnswerKey(LF_EIdReplace.OK), "the clerk hands the new card over");
		ctx.EqualStr("#LF-IdCard_ClerkNoCash", LF_IdCardClerk.AnswerKey(LF_EIdReplace.NOT_ENOUGH_CASH), "the clerk names the fee");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckSheet(LF_TestContext ctx)
	{
		array<ref LF_UIPart> valid = LF_IdCardView.BuildParts("Ada", "Novak", 31, "EV 000003", "09/06/1989", false, true);
		LF_UIPart portrait = FindPart(valid, LF_IdCardView.PORTRAIT);
		ctx.True(portrait != null && portrait.GetKind() == LF_EUIPartKind.PORTRAIT, "the card carries a portrait");
		if (portrait)
		{
			ctx.Equal(5, portrait.GetValues().Count(), "five printed lines beside the photo");
			ctx.EqualStr("Novak|Ada|31|09/06/1989|EV 000003", JoinValues(portrait), "surname, given names, age, date of issue, number");
			ctx.EqualStr(LF_IdCardView.STAMP_IMAGESET, portrait.GetText(), "the registry stamp is over the photo");
			ctx.True(portrait.GetValues().Count() <= LF_UIScreen.PORTRAIT_LINES, "the lines fit the layout");
		}

		ctx.True(FindPart(valid, LF_IdCardView.VOID_LINE) == null, "a valid card has no VOID line");
		ctx.EqualStr("#LF-ID_CardClose", FindPart(valid, "IdClose").GetText(), "a card shown to you is handed back");

		array<ref LF_UIPart> replaced = LF_IdCardView.BuildParts("Ada", "Novak", 31, "EV 000001", "", true, false);
		ctx.True(FindPart(replaced, LF_IdCardView.VOID_LINE) != null, "a replaced card reads VOID");
		ctx.EqualStr("Novak|Ada|31|-|EV 000001", JoinValues(FindPart(replaced, LF_IdCardView.PORTRAIT)), "an empty field prints a dash");
		ctx.EqualStr("#LF-IdCard_PutAway", FindPart(replaced, "IdClose").GetText(), "your own card is put away");

		string number;
		string issued;
		LF_IdCardView.UnpackIssue(LF_IdCardView.PackIssue("EV 000003", "09/06/1989"), number, issued);
		ctx.EqualStr("EV 000003/09/06/1989", number + "/" + issued, "the number and the date travel as one value");
		LF_IdCardView.UnpackIssue(LF_IdCardView.PackIssue("", ""), number, issued);
		ctx.EqualStr("/", number + "/" + issued, "a card with no number and no date stays empty");
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_UIPart FindPart(array<ref LF_UIPart> parts, string name)
	{
		foreach (LF_UIPart part : parts)
		{
			if (part.GetName() == name)
				return part;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static string JoinValues(LF_UIPart part)
	{
		if (!part)
			return "<none>";

		string joined;
		foreach (int i, string value : part.GetValues())
		{
			if (i > 0)
				joined += "|";

			joined += value;
		}

		return joined;
	}
}
