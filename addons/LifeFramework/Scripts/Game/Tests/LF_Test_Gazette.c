// red-proof: PENDING
// tier: LOGIC
//! Gazette (#792): bus events become headlines, a cap holds per kind and per paper, and a name is
//! withheld until the suspect is identified.
class LF_Test_Gazette : LF_Test
{
	override string GetName()
	{
		return "gazette/build";
	}

	//------------------------------------------------------------------------------------------------
	protected bool AnyRowHolds(notnull LF_GazetteEdition edition, string text)
	{
		foreach (LF_GazetteRow row : edition.m_aRows)
		{
			if (row.Pack().Contains(text))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_EventBus.ResetStatics();
		LF_Gazette.ResetStatics();
		LF_GazetteConfig.UseForTest(LF_GazetteConfig.MakeForTest(3, 1));
		LF_Gazette.SetTestName("u1", "Bob Ray");
		LF_Gazette.SetTestName("u2", "Ann Lee");
		LF_Gazette.SetTestName("u4", "Cy Dee");

		// Through the bus: a counted event waits, an uncounted one does not.
		LF_Gazette.Install();
		LF_EventBus.Publish(LF_OnCrimeCommitted.Create("u1", "#OFFENCE_Theft"));
		LF_EventBus.Publish(LF_OnMoneyMoved.Create("u1", 50, "shop-buy"));
		ctx.True(LF_Gazette.PendingCount() == 1, "a crime event waits for the edition and a money move does not");
		LF_EventBus.Publish(LF_OnSuspectIdentified.Create("u2", "id check", 1));
		ctx.True(LF_Gazette.IsIdentified("u2") && !LF_Gazette.IsIdentified("u1"), "an identification event marks only that suspect");

		// Names: u1 is unidentified, u2 identified, u4 arrested (public).
		LF_EventBus.Publish(LF_OnCrimeCommitted.Create("u2", "#OFFENCE_Robbery"));
		LF_EventBus.Publish(LF_OnArrested.Create("u4", 600, 2));
		LF_EventBus.Publish(LF_OnJobCompleted.Create("u5", 1, 120));
		LF_EventBus.Publish(LF_OnJobCompleted.Create("u5", 1, 90));

		LF_GazetteEdition edition = LF_Gazette.BuildEdition(1000);
		ctx.True(edition != null, "an edition is built from the waiting events");
		if (!edition)
			return;

		ctx.True(edition.m_iNumber == 1, "the first edition is number 1");
		ctx.True(edition.m_aRows.Count() == 3, "the paper holds at most 3 headlines");
		ctx.True(edition.m_aRows[0].m_sKind == "arrest", "the arrest outranks the crime and the job");
		ctx.True(LF_Gazette.PendingCount() == 0, "building an edition consumes the waiting events");

		int crimes;
		int jobs;
		foreach (LF_GazetteRow row : edition.m_aRows)
		{
			if (row.m_sKind == "crime")
				crimes++;
			if (row.m_sKind == "job")
				jobs++;
		}

		ctx.True(crimes == 1, "one kind is capped to 1 story though 2 crimes were reported");
		ctx.True(jobs == 1, "the job kind is capped to 1 story though 2 jobs were done");
		ctx.True(AnyRowHolds(edition, "Cy Dee"), "an arrest prints the name: it is public");
		ctx.True(AnyRowHolds(edition, "Ann Lee"), "the newest crime is the identified suspect's and prints the name");
		ctx.False(AnyRowHolds(edition, "Bob Ray"), "an unidentified suspect's name is never printed");
		ctx.False(AnyRowHolds(edition, "u5"), "a job story names nobody");

		// Withheld: the crime alone, suspect unidentified.
		LF_EventBus.Publish(LF_OnCrimeCommitted.Create("u1", "#OFFENCE_Theft"));
		LF_GazetteEdition anon = LF_Gazette.BuildEdition(2000);
		ctx.True(anon != null && anon.m_aRows.Count() == 1, "an unidentified crime still makes a story");
		if (anon)
		{
			ctx.True(anon.m_aRows[0].m_sTextKey == "#LF-Gazette_CrimeAnon", "the story uses the anonymous wording");
			ctx.True(anon.m_aRows[0].Arg(0).IsEmpty(), "the name argument is empty");
			ctx.True(anon.m_aRows[0].Arg(1) == "#OFFENCE_Theft", "the offence key rides as an argument, translated at the reader");
			LF_GazetteRow back = LF_GazetteRow.Unpack(anon.m_aRows[0].Pack());
			ctx.True(back != null && back.m_sTextKey == anon.m_aRows[0].m_sTextKey && back.Arg(1) == "#OFFENCE_Theft", "a row survives the pack for the reply RPC");
		}

		// A quiet week prints nothing and takes no edition number; only the last N editions stay.
		ctx.True(LF_Gazette.BuildEdition(3000) == null, "no events, no edition");
		LF_EventBus.Publish(LF_OnArrested.Create("u4", 60, 1));
		LF_Gazette.BuildEdition(4000);
		ctx.True(LF_Gazette.EditionCount() == 2 && LF_Gazette.Latest().m_iNumber == 3, "two editions are kept and the quiet week took no number");

		LF_Gazette.ResetStatics();
		ctx.True(LF_Gazette.PendingCount() == 0 && LF_Gazette.EditionCount() == 0, "the static reset clears the paper");
	}
}
