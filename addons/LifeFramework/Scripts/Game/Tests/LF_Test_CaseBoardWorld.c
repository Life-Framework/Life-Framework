// red-proof: Observed 2026-11-13 with `tools/cli test --no-build --only LF_Test_CaseBoardWorld`: LF_EvidenceLocker.LogBag handing the case board an empty note failed `- the card carries the note the bag carried (expected '#AR-Boots_Jungle', got '')`. Restored; green.
// tier: WORLD
//! Case board (#148) in the world: the board prefab spawns with its component, its two actions and
//! its Read card; and a stamped bag logged at the locker becomes a card that Pin puts
//! on the board with the note the piece carried (the locker is the record; the board adds the pin).
class LF_Test_CaseBoardWorld : LF_Test
{
	static const ResourceName BOARD_PREFAB = "{C8B78C628082CA0D}Prefabs/Investigation/LF_CaseBoard.et";
	protected static const string ROBBER = "caseboard-world-robber";

	override string GetName()
	{
		return "investigation/case-board-world";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity board = LF_Utils.SpawnEntityPrefab(BOARD_PREFAB, "1000 0 1010");
		ctx.NotNull(board, "the case board prefab spawns");
		if (board)
		{
			ctx.NotNull(board.FindComponent(LF_CaseBoardComponent), "the board carries LF_CaseBoardComponent");
			bool pin;
			bool unpin;
			BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(board.FindComponent(BaseActionsManagerComponent));
			if (manager)
			{
				array<BaseUserAction> actions = {};
				manager.GetActionsList(actions);
				foreach (BaseUserAction action : actions)
				{
					if (LF_CaseBoardPinAction.Cast(action))
						pin = true;

					if (LF_CaseBoardUnpinAction.Cast(action))
						unpin = true;
				}
			}

			ctx.True(pin, "the board offers Pin evidence");
			ctx.True(unpin, "the board offers Unpin evidence");
			SCR_EntityHelper.DeleteEntityAndChildren(board);
		}

		ctx.True(LF_SignCatalog.IsUsable(LF_SignCatalog.Find("police_cases")), "the board's Read card is in LF_Signs.conf");

		array<ref LF_WantedReportRecord> reportsBefore = LF_WantedReports.ExportAll();
		array<ref LF_EvidenceLockerRow> lockerBefore = LF_EvidenceLocker.ExportAll();
		array<ref LF_CaseBoardRow> boardBefore = LF_CaseBoard.ExportAll();
		LF_WantedReports.ApplyAll(null);
		LF_EvidenceLocker.ApplyAll(null);
		LF_CaseBoard.ResetStatics();

		LF_WantedReportRecord robbery = LF_WantedReports.Add(ROBBER, LF_WantedReports.OFFENCE_ROBBERY, "", "", "Fuel station", false);
		LF_CaseBoard.OnReport(robbery.m_iSequence);
		IEntity bag = LF_Utils.SpawnEntityPrefab(LF_Evidence.BAG_PREFAB, "1000 0 1000");
		LF_EvidenceBagComponent bagComponent;
		if (bag)
			bagComponent = LF_EvidenceBagComponent.Cast(bag.FindComponent(LF_EvidenceBagComponent));

		ctx.NotNull(bagComponent, "the bag spawns with its component");
		if (bagComponent)
		{
			bagComponent.Stamp(robbery.m_iSequence, LF_WantedReports.OFFENCE_ROBBERY, "Fuel station", LF_Evidence.KIND_FOOTPRINT, 2, "#AR-Boots_Jungle");
			ctx.NotNull(LF_EvidenceLocker.LogBag(bag, "caseboard-world-officer"), "the locker logs the bag");
			ctx.Equal(0, LF_CaseBoard.PinnedCount(robbery.m_iSequence), "a logged bag is a card in waiting, not on the board");
			ctx.Equal(1, LF_CaseBoard.Pin(null), "Pin puts the logged card up");
			array<string> entries = {};
			LF_CaseBoard.BuildEntries(entries);
			array<ref LF_CaseBoardCase> cases = {};
			LF_CaseBoard.Decode(entries, cases);
			ctx.Equal(1, cases.Count(), "the robbery is the case on the board");
			if (cases.Count() == 1 && cases[0].m_aKinds.Count() == 1)
			{
				ctx.EqualStr(LF_Evidence.KIND_FOOTPRINT, cases[0].m_aKinds[0], "the card is the footprint");
				ctx.EqualStr("#AR-Boots_Jungle", cases[0].m_aNotes[0], "the card carries the note the bag carried");
			}
			else
			{
				ctx.True(false, string.Format("expected one case with one card, got %1 entries", entries.Count()));
			}
		}

		LF_WantedReports.ApplyAll(reportsBefore);
		LF_EvidenceLocker.ApplyAll(lockerBefore);
		LF_CaseBoard.ResetStatics();
		LF_CaseBoard.ApplyAll(boardBefore);
	}
}
