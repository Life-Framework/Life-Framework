// red-proof: Observed 2026-11-13 with `tools/cli test --no-build --only LF_Test_CaseBoardPersist`: commenting out `LF_CaseBoard.ApplyAll(caseRows)` in the load failed `- the pinned cards are back (expected 2, got 0)` and `- the time of the crime is back (expected '09:36', got '')`. Restored; green. Restart path stays with wt e2e.
// tier: PERSISTENCE
//! The case board rides the real save path (the "caseboard" payload, save version 31, payload 31):
//! the time of the crime, the pinned cards and their notes are back after the store is wiped and read
//! back, the pin order goes on after the highest loaded pin, and loading twice adds nothing.
class LF_Test_CaseBoardPersist : LF_Test
{
	protected static const string ROBBER = "caseboard-save-robber";

	override string GetName()
	{
		return "investigation/case-board-persist";
	}

	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));

		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

		array<ref LF_WantedReportRecord> reportsBefore = LF_WantedReports.ExportAll();
		array<ref LF_CaseBoardRow> boardBefore = LF_CaseBoard.ExportAll();
		LF_WantedReports.ApplyAll(null);
		LF_CaseBoard.ResetStatics();

		LF_WantedReportRecord robbery = LF_WantedReports.Add(ROBBER, LF_WantedReports.OFFENCE_ROBBERY, "", "", "Fuel station", false);
		LF_CaseBoard.OnReport(robbery.m_iSequence);
		string time = LF_CaseBoard.TimeOf(robbery.m_iSequence);
		LF_CaseBoard.OnLogged(LockerRow(31, robbery.m_iSequence, LF_Evidence.KIND_STATEMENT), "#AR-Jacket_M88~~AB");
		LF_CaseBoard.OnLogged(LockerRow(32, robbery.m_iSequence, LF_Evidence.KIND_CASING), "");
		LF_CaseBoard.OnLogged(LockerRow(33, robbery.m_iSequence, LF_Evidence.KIND_FOOTPRINT), "#AR-Boots_Jungle");
		LF_CaseBoard.Pin(null);
		LF_CaseBoard.Pin(null);
		ctx.Equal(2, LF_CaseBoard.PinnedCount(robbery.m_iSequence), "two cards are pinned before the save");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains("#AR-Jacket_M88~~AB"), "the save carries the statement card's note");

		LF_CaseBoard.ResetStatics();
		ctx.Equal(0, LF_CaseBoard.PinnedCount(robbery.m_iSequence), "the restarted session has an empty board");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(32, version, "the save is written at version 32 (caseboard is payload 31)");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "caseboard");
		ctx.Equal(2, LF_CaseBoard.PinnedCount(robbery.m_iSequence), "the pinned cards are back");
		ctx.EqualStr(time, LF_CaseBoard.TimeOf(robbery.m_iSequence), "the time of the crime is back");

		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, again, "caseboard");
		ctx.Equal(4, LF_CaseBoard.ExportAll().Count(), "loading twice gives the same board (one time row, three cards)");

		ctx.Equal(1, LF_CaseBoard.Pin(null), "the card left in waiting can still be pinned");
		array<LF_CaseBoardRow> cards = {};
		LF_CaseBoard.PinnedCards(robbery.m_iSequence, cards);
		ctx.Equal(3, cards.Count(), "three cards are up");
		if (cards.Count() == 3)
		{
			ctx.Equal(33, cards[2].m_iNumber, "the pin after the load comes after the loaded pins");
			ctx.EqualStr("#AR-Boots_Jungle", cards[2].m_sNote, "its note came back");
		}

		LF_WantedReports.ApplyAll(reportsBefore);
		LF_CaseBoard.ResetStatics();
		LF_CaseBoard.ApplyAll(boardBefore);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_EvidenceLockerRow LockerRow(int number, int report, string kind)
	{
		LF_EvidenceLockerRow row = new LF_EvidenceLockerRow();
		row.m_iNumber = number;
		row.m_iReport = report;
		row.m_sKind = kind;
		return row;
	}
}
