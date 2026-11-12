// red-proof: Observed 2026-11-08 with `tools/cli test --no-build --only` (three breaks in one run, 0/3 passed),
// then restored; green. LF_FineTicketComponent.Print writing `m_bPaid = false` failed
// `[LFTEST] FAIL police/fine-ticket`, `- the carried ticket is stamped paid`.
// A new prefab needs `tools/cli build` first (FineTicket.et is not in main's resourceDatabase.rdb).

// tier: WORLD
//! police/fine-ticket: the ticket item exists and reads like a ticket, the desk takes carried cash
//! once and stamps it a receipt, the clock reprints the paper, a respawn hands an unpaid ticket
//! back, the old remote fine is gone, and the desk carries Pay ticket.
class LF_Test_FineTicket : LF_Test
{
	protected static const string HOLDER = "fine-ticket-holder";
	protected static const string OFFICER = "fine-ticket-officer";
	protected static const ResourceName DESK = "{034D1397A95787DB}Prefabs/Police/LF_PoliceDutyDesk.et";

	override string GetName()
	{
		return "police/fine-ticket";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_FineTicketRecord> before = LF_FineTickets.ExportAll();
		LF_FineTickets.ApplyAll(null);
		LF_FineTickets.Configure(120, 120, 2);

		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (!body)
		{
			Restore(before);
			return;
		}

		LF_FineTickets.BindBody(HOLDER, body);
		LF_FineTicketRecord record = LF_FineTickets.Issue(HOLDER, "Ada Holder", OFFICER, "Bo Officer", LF_PoliceRanks.SENIOR, "#LF-Ticket_OffenceSpeeding", 200, "08/11/1989 12:00");
		LF_FineTicketComponent ticket = LF_FineTicketItem.Grant(body, record);
		ctx.NotNull(ticket, "the ticket prefab spawns with LF_FineTicketComponent");
		array<LF_FineTicketComponent> carried = {};
		LF_FineTicketItem.FindCarried(body, carried);
		ctx.Equal(1, carried.Count(), "the holder carries one ticket");
		if (!ticket || carried.Count() != 1)
		{
			LF_TestCharacterFixture.Delete(body);
			Restore(before);
			return;
		}

		ctx.Equal(record.m_iNumber, ticket.GetNumber(), "the paper carries the ticket number");
		ctx.Equal(200, ticket.GetAmount(), "the paper carries the amount");
		ctx.Equal(2, ticket.GetMinutesLeft(), "120 online seconds print as 2 minutes");
		ctx.True(LF_FineTicketItem.FirstUnpaid(body) == ticket, "the desk sees the unpaid ticket");
		string text = ticket.Describe();
		ctx.True(text.Contains("Ada Holder"), "the ticket names its holder");
		ctx.True(text.Contains("Bo Officer"), "the ticket names the officer");
		ctx.True(text.Contains("200"), "the ticket prints the amount");
		ctx.True(text.Contains("400"), "the ticket says what it rises to");

		// The clock reprints the paper when the minute changes and when it doubles.
		array<string> online = {HOLDER};
		LF_FineTickets.TickOnline(61, online);
		ctx.Equal(1, ticket.GetMinutesLeft(), "the paper counts the minutes down");
		LF_FineTickets.TickOnline(60, online);
		ctx.Equal(400, ticket.GetAmount(), "the paper shows the doubled amount at the first deadline");
		ctx.Equal(LF_FineTicketRules.STAGE_LATE, ticket.GetStage(), "the paper says it is overdue");

		// The desk: carried cash only, once, and the paper becomes a receipt.
		LF_MoneyUtils.RemoveCash(body, LF_MoneyUtils.GetCash(body));
		LF_MoneyUtils.AddCash(body, 300);
		LF_FineTicketRole.PayAs(body, HOLDER);
		ctx.Equal(300, LF_MoneyUtils.GetCash(body), "300 in the pocket does not pay a 400 ticket and takes nothing");
		ctx.False(ticket.IsPaid(), "the refused ticket is still unpaid");
		LF_MoneyUtils.AddCash(body, 200);
		ctx.True(LF_FineTicketRole.PayAs(body, HOLDER) == LF_ETicketPay.PAID, "500 in the pocket pays it at the desk");
		ctx.Equal(100, LF_MoneyUtils.GetCash(body), "the desk takes the doubled 400 from carried cash");
		ctx.True(ticket.IsPaid(), "the carried ticket is stamped paid");
		ctx.True(LF_FineTicketRole.PayAs(body, HOLDER) == LF_ETicketPay.NO_TICKET, "paying again finds no unpaid ticket");
		ctx.Equal(100, LF_MoneyUtils.GetCash(body), "the second payment takes nothing");
		ctx.True(LF_FineTicketItem.FirstUnpaid(body) == null, "the desk offers nothing more");

		// A respawn hands an unpaid ticket back and prints a restart's blank copy.
		LF_FineTicketRecord second = LF_FineTickets.Issue(HOLDER, "Ada Holder", OFFICER, "Bo Officer", LF_PoliceRanks.SENIOR, "#LF-Ticket_OffenceDisturbance", 150, "08/11/1989 12:30");
		LF_FineTickets.OnPlayerSpawned(HOLDER, body);
		ctx.True(LF_FineTicketItem.FindNumber(body, second.m_iNumber) != null, "a spawn hands over an unpaid ticket the body lacks");
		LF_FineTickets.OnPlayerSpawned(HOLDER, body);
		LF_FineTicketItem.FindCarried(body, carried);
		ctx.Equal(2, carried.Count(), "a second spawn hands no duplicate");

		// The desk prefab carries the payment action.
		Resource desk = Resource.Load(DESK);
		ctx.True(desk && desk.IsValid(), "the police desk prefab loads");
		IEntity deskEntity;
		if (desk && desk.IsValid())
			deskEntity = GetGame().SpawnEntityPrefab(desk, GetGame().GetWorld());

		ctx.True(deskEntity && LF_ActionSite.FindNear(deskEntity.GetOrigin(), 3, "LF_FineTicketPayAction") != null, "the desk punch clock offers Pay ticket");
		if (deskEntity)
			SCR_EntityHelper.DeleteEntityAndChildren(deskEntity);

		LF_TestCharacterFixture.Delete(body);
		Restore(before);
	}

	protected void Restore(array<ref LF_FineTicketRecord> before)
	{
		LF_FineTickets.BindBody(HOLDER, null);
		LF_FineTickets.ApplyAll(before);
		LF_FineTickets.UseConfig();
	}
}
