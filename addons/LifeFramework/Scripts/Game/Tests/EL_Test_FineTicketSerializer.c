// red-proof: Observed 2026-11-08 with `tools/cli test --no-build --only` (three breaks in one run, 0/3 passed),
// then restored; green. Commenting out LF_FineTickets.ApplyAll in the serializer's tickets read failed
// `[ELTEST] FAIL police/fine-ticket-serializer`, `- the ticket is back after the load`.

// tier: WORLD
//! Outstanding tickets ride the real save path (the "tickets" payload, version 12): amount, clock,
//! stage and paid flag come back after the store is wiped.
class EL_Test_FineTicketSerializer : EL_Test
{
	protected static const string HOLDER = "ticket-save-holder";

	override string GetName()
	{
		return "police/fine-ticket-serializer";
	}

	override void Run(EL_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves");
		if (!gameMode)
			return;

		EL_PersistenceComponent persistence = EL_PersistenceComponent.Cast(gameMode.FindComponent(EL_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries EL_PersistenceComponent");
		if (!persistence)
			return;

		array<ref LF_FineTicketRecord> before = LF_FineTickets.ExportAll();
		LF_FineTickets.ApplyAll(null);
		LF_FineTickets.Configure(60, 60, 2, 1);
		LF_FineTicketRecord open = LF_FineTickets.Issue(HOLDER, "Ada Holder", "ticket-save-officer", "Bo Officer", LF_PoliceRanks.PATROL, "#LF-Ticket_OffenceSpeeding", 200, "08/11/1989 12:00");
		array<string> online = {HOLDER};
		LF_FineTickets.TickOnline(75, online);
		int number = open.m_iNumber;

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(EL_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(HOLDER), "the save carries the tickets payload");

		// Restart: the session has no tickets.
		LF_FineTickets.ApplyAll(null);
		ctx.Equal(0, LF_FineTickets.CountOpen(HOLDER), "the restarted session starts without the ticket");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(12, version, "the save is written at version 12 (tickets is payload 12)");
		EL_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "tickets");
		LF_FineTicketRecord loaded = LF_FineTickets.Find(number);
		ctx.NotNull(loaded, "the ticket is back after the load");
		if (loaded)
		{
			ctx.Equal(400, loaded.m_iAmount, "the doubled amount is back");
			ctx.Equal(200, loaded.m_iBaseAmount, "the written amount is back");
			ctx.Equal(LF_FineTicketRules.STAGE_LATE, loaded.m_iStage, "the stage is back, so it does not double again");
			ctx.EqualFloat(75, loaded.m_fOnlineSeconds, 0.01, "the online clock is back");
			ctx.False(loaded.m_bPaid, "it is still unpaid");
			ctx.EqualStr("Bo Officer", loaded.m_sOfficerName, "the officer is back");
		}

		LF_FineTickets.ApplyAll(before);
		LF_FineTickets.UseConfig();
	}
}
