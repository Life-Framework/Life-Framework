// red-proof: with `RedProof()` on the encoder and decoder are not asked, so no entry is built and no case decodes, and the
// shape, separator, flag, card and cap checks fail.

//! police/case-board-encode: the entries the case board sends officers (LF_CaseBoard.EncodeCase, EncodeCard, Decode). A case
//! entry is "C;report;offence;place;time;name;look;flag", a card entry "E;report;kind;note". A separator or newline typed into
//! a text cannot split an entry into the wrong fields (it becomes a comma or a space), the identified flag is 1 or 0, an
//! encoded list decodes back to the same cases with their cards in order, a case holds at most four cards, the board shows at
//! most three cases, and a card of another report is not glued to the case before it.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_CaseBoardEncode : LF_Test
{
	override string GetName()
	{
		return "police/case-board-encode";
	}

	override void Run(LF_TestContext ctx)
	{
		string caseEntry = "?";
		string openCaseEntry = "?";
		string cardEntry = "?";
		string splitCase = "?";
		string splitCard = "?";
		int decoded = -1;
		int firstReport = -1;
		string firstOffence = "?";
		string firstPlace = "?";
		string firstName = "?";
		bool firstIdentified = false;
		int firstCards = -1;
		string firstCardKind = "?";
		string firstCardNote = "?";
		bool secondIdentified = true;
		int secondCards = -1;
		int cappedCards = -1;
		int cappedCases = -1;
		if (!RedProof())
		{
			caseEntry = LF_CaseBoard.EncodeCase(7, "#LF-Offence_Robbery", "Harbour", "09:30", "Sam Hauler", "tall, red coat", true);
			openCaseEntry = LF_CaseBoard.EncodeCase(8, "Theft", "Pier", "10:00", "", "", false);
			cardEntry = LF_CaseBoard.EncodeCard(7, "witness", "saw a red truck", 3);
			splitCase = LF_CaseBoard.EncodeCase(9, "Rob;bery", "Har;bour", "09:30", "Sam\nHauler", "x", false);
			splitCard = LF_CaseBoard.EncodeCard(9, "wit;ness", "line one\nline two", 1);

			array<string> entries = {};
			entries.Insert(caseEntry);
			entries.Insert(LF_CaseBoard.EncodeCard(7, "witness", "saw a red truck", 1));
			entries.Insert(LF_CaseBoard.EncodeCard(7, "print", "left on the door", 2));
			entries.Insert(openCaseEntry);
			array<ref LF_CaseBoardCase> cases = {};
			decoded = LF_CaseBoard.Decode(entries, cases);
			if (decoded >= 2)
			{
				firstReport = cases[0].m_iReport;
				firstOffence = cases[0].m_sOffence;
				firstPlace = cases[0].m_sPlace;
				firstName = cases[0].m_sName;
				firstIdentified = cases[0].m_bIdentified;
				firstCards = cases[0].m_aKinds.Count();
				firstCardKind = cases[0].m_aKinds[0];
				firstCardNote = cases[0].m_aNotes[0];
				secondIdentified = cases[1].m_bIdentified;
				secondCards = cases[1].m_aKinds.Count();
			}

			array<string> crowded = {};
			crowded.Insert(LF_CaseBoard.EncodeCase(1, "a", "p", "t", "n", "l", false));
			for (int card = 0; card < LF_CaseBoard.CARDS_PER_CASE + 2; card++)
			{
				crowded.Insert(LF_CaseBoard.EncodeCard(1, "k" + card.ToString(), "note", card));
			}

			array<ref LF_CaseBoardCase> crowdedCases = {};
			LF_CaseBoard.Decode(crowded, crowdedCases);
			cappedCards = crowdedCases[0].m_aKinds.Count();

			array<string> many = {};
			for (int c = 0; c < LF_CaseBoard.MAX_CASES + 2; c++)
			{
				many.Insert(LF_CaseBoard.EncodeCase(c + 1, "a", "p", "t", "n", "l", false));
			}

			array<ref LF_CaseBoardCase> manyCases = {};
			cappedCases = LF_CaseBoard.Decode(many, manyCases);
		}

		ctx.EqualStr("C;7;#LF-Offence_Robbery;Harbour;09:30;Sam Hauler;tall, red coat;1", caseEntry, "a case entry has eight fields in order and the identified flag 1");
		ctx.EqualStr("C;8;Theft;Pier;10:00;;;0", openCaseEntry, "an unidentified case sends empty name and look and the flag 0");
		ctx.EqualStr("E;7;witness;saw a red truck", cardEntry, "a card entry is E, the report, the kind and the note (the row number stays out of it)");
		ctx.EqualStr("C;9;Rob,bery;Har,bour;09:30;Sam Hauler;x;0", splitCase, "a separator becomes a comma and a newline a space, so the entry keeps eight fields");
		ctx.EqualStr("E;9;wit,ness;line one line two", splitCard, "a card entry keeps four fields whatever was typed");
		ctx.Equal(2, decoded, "two case entries decode to two cases");
		ctx.Equal(7, firstReport, "the first case keeps its report number");
		ctx.EqualStr("#LF-Offence_Robbery", firstOffence, "the offence key travels as written");
		ctx.EqualStr("Harbour", firstPlace, "the place decodes");
		ctx.EqualStr("Sam Hauler", firstName, "an identified case decodes the name");
		ctx.True(firstIdentified, "the identified flag decodes");
		ctx.Equal(2, firstCards, "both cards after the first case belong to it");
		ctx.EqualStr("witness", firstCardKind, "the cards keep their order");
		ctx.EqualStr("saw a red truck", firstCardNote, "the card note decodes");
		ctx.False(secondIdentified, "the second case is not identified");
		ctx.Equal(0, secondCards, "the second case has none of the first case's cards");
		ctx.Equal(LF_CaseBoard.CARDS_PER_CASE, cappedCards, "a case shows at most four cards");
		ctx.Equal(LF_CaseBoard.MAX_CASES, cappedCases, "the board shows at most three cases");
	}
}
