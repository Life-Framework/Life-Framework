// red-proof: RED-OK quests/clerk-first-steps, observed 2026-11-22 with `tools\cli test --no-build --red LF_Test_ClerkFirstSteps`: with the hook on the licence row shows for a newcomer with no card and the character change is offered to an unregistered first character (both assertions fail); green with `--only`.
//! The first minutes at the town hall (first-2-hours review A7, A10): the driving-licence row waits for the
//! ID card so the clerk's Talk row comes first, the clerk offers no character change to a brand-new
//! arrival.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ClerkFirstSteps : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "quests/clerk-first-steps";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// A7: no papers, no licence row. With a card, an unlicensed player sees it; a holder never does.
		bool newcomerRow = true;
		if (!RedProof())
			newcomerRow = LF_LicenceDesk.DrivingRowShown(true, false, 0);

		ctx.False(newcomerRow, "a newcomer with no ID card is not offered the driving licence (the clerk's Talk row is first)");
		ctx.True(LF_LicenceDesk.DrivingRowShown(true, false, 1), "with an ID card the licence row shows");
		ctx.False(LF_LicenceDesk.DrivingRowShown(true, true, 1), "a licence holder gets no row");
		ctx.False(LF_LicenceDesk.DrivingRowShown(false, false, 1), "the License system off hides the row");

		// A10: a brand-new arrival is not offered "Change character" as the first thing to do.
		bool switchForNew = true;
		if (!RedProof())
			switchForNew = LF_CharacterSwitch.SwitchOffered(3, true, 1, false);

		ctx.False(switchForNew, "an unregistered first character is not offered a character change");
		ctx.True(LF_CharacterSwitch.SwitchOffered(3, true, 1, true), "a registered character may register another file");
		ctx.True(LF_CharacterSwitch.SwitchOffered(3, true, 2, false), "an account with a second file may change over");
		ctx.False(LF_CharacterSwitch.SwitchOffered(1, true, 1, true), "a server with one file per account never offers it");
		ctx.False(LF_CharacterSwitch.SwitchOffered(3, false, 2, true), "no active character, no offer");
	}
}
