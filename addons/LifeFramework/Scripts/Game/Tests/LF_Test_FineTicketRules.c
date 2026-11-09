// red-proof: Observed 2026-11-08 with `tools/cli test --no-build --only` (three breaks in one run, 0/3 passed),
// then restored; green. LF_FineTicketRules.LateAmount returning the written amount failed
// `[LFTEST] FAIL police/fine-ticket-rules`, `- the first deadline doubles the amount (expected 400, got 200)`.

// tier: LOGIC
//! The paper ticket's rules without a world: rank cap, online-time deadlines (doubles once, then
//! +1 wanted once), payment from carried cash only and only once, and the save records.
class LF_Test_FineTicketRules : LF_Test
{
	protected static const string HOLDER = "ticket-rules-holder";
	protected static const string OFFLINE = "ticket-rules-offline";
	protected static const string OFFICER = "ticket-rules-officer";

	override string GetName()
	{
		return "police/fine-ticket-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		// Rank cap and the open-ticket limit.
		ctx.True(LF_FineTicketRules.CheckWrite(true, 1000, 1000, true, true, false, 0, 3) == LF_ETicketRefusal.NONE, "a patrol officer writes an offence at the cap");
		ctx.True(LF_FineTicketRules.CheckWrite(true, 1000, 1500, true, true, false, 0, 3) == LF_ETicketRefusal.ABOVE_CAP, "an offence above the rank cap is refused");
		ctx.True(LF_FineTicketRules.CheckWrite(false, 5000, 200, true, true, false, 0, 3) == LF_ETicketRefusal.NOT_OFFICER, "an off-duty player writes no ticket");
		ctx.True(LF_FineTicketRules.CheckWrite(true, 1000, 200, false, true, false, 0, 3) == LF_ETicketRefusal.BAD_OFFENCE, "an offence index outside the list is refused");
		ctx.True(LF_FineTicketRules.CheckWrite(true, 1000, 200, true, true, true, 0, 3) == LF_ETicketRefusal.SELF, "an officer cannot ticket themselves");
		ctx.True(LF_FineTicketRules.CheckWrite(true, 1000, 200, true, true, false, 3, 3) == LF_ETicketRefusal.TOO_MANY_OPEN, "a fourth unpaid ticket is refused");

		LF_PlayerAccount patrol = LF_PlayerAccountManager.GetOrCreate(OFFICER);
		patrol.SetFaction(LF_Faction.POLICE);
		patrol.SetPoliceRank(LF_PoliceRanks.PATROL);
		patrol.SetOnDuty(true);
		array<ref LF_TicketOffence> offences = {};
		LF_PoliceConfig.TicketOffences(offences);
		ctx.True(offences.Count() >= 2, "the config lists ticket offences");
		int aboveCap = -1;
		int withinCap = -1;
		foreach (int i, LF_TicketOffence offence : offences)
		{
			if (offence.m_iAmount > LF_PoliceRanks.FineCap(LF_PoliceRanks.PATROL) && aboveCap < 0)
				aboveCap = i;

			if (offence.m_iAmount <= LF_PoliceRanks.FineCap(LF_PoliceRanks.PATROL) && withinCap < 0)
				withinCap = i;
		}

		LF_TicketOffence picked;
		ctx.True(withinCap >= 0 && LF_FineTickets.CheckWrite(patrol, OFFICER, HOLDER, withinCap, picked) == LF_ETicketRefusal.NONE, "a patrol officer may write a listed offence within the cap");
		ctx.True(aboveCap >= 0 && LF_FineTickets.CheckWrite(patrol, OFFICER, HOLDER, aboveCap, picked) == LF_ETicketRefusal.ABOVE_CAP, "the flagship list has an offence only a senior officer may write");
		patrol.SetPoliceRank(LF_PoliceRanks.SENIOR);
		ctx.True(LF_FineTickets.CheckWrite(patrol, OFFICER, HOLDER, aboveCap, picked) == LF_ETicketRefusal.NONE, "a senior officer writes it");

		// Deadlines in online time.
		ctx.Equal(LF_FineTicketRules.STAGE_OPEN, LF_FineTicketRules.StageFor(59, 60, 60), "before the first deadline the ticket is open");
		ctx.Equal(LF_FineTicketRules.STAGE_LATE, LF_FineTicketRules.StageFor(60, 60, 60), "at the first deadline it is late");
		ctx.Equal(LF_FineTicketRules.STAGE_WANTED, LF_FineTicketRules.StageFor(120, 60, 60), "at the second deadline it is a warrant");
		ctx.Equal(400, LF_FineTicketRules.LateAmount(200, 2), "x2 doubles the amount");
		ctx.Equal(200, LF_FineTicketRules.LateAmount(200, 0.5), "a multiplier below 1 never lowers it");
		ctx.Equal(2, LF_FineTicketRules.MinutesLeft(61), "61 seconds read 2 minutes");

		array<ref LF_FineTicketRecord> before = LF_FineTickets.ExportAll();
		LF_FineTickets.ApplyAll(null);
		LF_FineTickets.Configure(60, 60, 2, 1);
		LF_PlayerAccount holder = LF_PlayerAccountManager.GetOrCreate(HOLDER);
		holder.SetWantedLevel(0);
		LF_PlayerAccount offline = LF_PlayerAccountManager.GetOrCreate(OFFLINE);
		offline.SetWantedLevel(0);

		LF_FineTicketRecord ticket = LF_FineTickets.Issue(HOLDER, "Ada Holder", OFFICER, "Bo Officer", LF_PoliceRanks.PATROL, "#LF-Ticket_OffenceSpeeding", 200, "08/11/1989 12:00");
		LF_FineTicketRecord away = LF_FineTickets.Issue(OFFLINE, "Cy Away", OFFICER, "Bo Officer", LF_PoliceRanks.PATROL, "#LF-Ticket_OffenceSpeeding", 200, "08/11/1989 12:00");
		ctx.NotNull(ticket, "a ticket is recorded");
		ctx.Equal(ticket.m_iNumber + 1, away.m_iNumber, "tickets are numbered in order");

		array<string> online = {HOLDER};
		LF_FineTickets.TickOnline(59, online);
		ctx.Equal(200, ticket.m_iAmount, "59 online seconds leave the amount");
		LF_FineTickets.TickOnline(1, online);
		ctx.Equal(400, ticket.m_iAmount, "the first deadline doubles the amount");
		ctx.Equal(200, ticket.m_iBaseAmount, "the written amount is kept");
		ctx.Equal(0, holder.GetWantedLevel(), "the first deadline adds no wanted level");
		LF_FineTickets.TickOnline(59, online);
		ctx.Equal(400, ticket.m_iAmount, "it doubles once, not again");
		LF_FineTickets.TickOnline(1, online);
		ctx.Equal(LF_FineTicketRules.STAGE_WANTED, ticket.m_iStage, "the second deadline is reached");
		ctx.Equal(1, holder.GetWantedLevel(), "the second deadline adds one wanted level");
		bool posted = false;
		array<ref LF_WantedReportRecord> reports = LF_WantedReports.GetFor(HOLDER);
		if (reports)
		{
			foreach (LF_WantedReportRecord report : reports)
			{
				if (report.m_sOffence == LF_FineTickets.OFFENCE_UNPAID && report.m_bIdentified)
					posted = true;
			}
		}

		ctx.True(posted, "the wanted board gets a named 'unpaid fine' poster");
		LF_FineTickets.TickOnline(600, online);
		ctx.Equal(1, holder.GetWantedLevel(), "the warrant is added once");
		ctx.Equal(400, ticket.m_iAmount, "the amount stays doubled");
		ctx.EqualFloat(0, away.m_fOnlineSeconds, 0.001, "an offline holder's clock does not move");
		ctx.Equal(200, away.m_iAmount, "an offline holder's ticket does not double");

		// Payment: carried cash only, all or nothing, once.
		LF_TestCashWallet poor = LF_TestCashWallet.Create(399, 10000);
		LF_FineTicketRecord paid;
		ctx.True(LF_FineTickets.Pay(HOLDER, poor, "08/11/1989 13:00", paid) == LF_ETicketPay.NOT_ENOUGH_CASH, "399 does not pay a 400 ticket");
		ctx.Equal(399, poor.Carried(), "a refused payment takes nothing");
		LF_TestCashWallet wallet = LF_TestCashWallet.Create(1000, 10000);
		ctx.True(LF_FineTickets.Pay(HOLDER, wallet, "08/11/1989 13:00", paid) == LF_ETicketPay.PAID, "the holder pays at the desk");
		ctx.Equal(600, wallet.Carried(), "the doubled amount is taken from carried cash");
		ctx.True(paid == ticket && ticket.m_bPaid, "the ticket is marked paid");
		ctx.EqualStr("08/11/1989 13:00", ticket.m_sPaidAt, "the receipt carries the payment time");
		ctx.True(LF_FineTickets.Pay(HOLDER, wallet, "08/11/1989 13:01", paid) == LF_ETicketPay.NO_TICKET, "a paid ticket cannot be paid twice");
		ctx.Equal(600, wallet.Carried(), "the second attempt takes nothing");
		ctx.Equal(1, holder.GetWantedLevel(), "paying late does not lift the warrant");
		ctx.Equal(0, LF_FineTickets.CountOpen(HOLDER), "the holder has no unpaid ticket left");

		// Save records: export and apply give the same store, twice.
		array<ref LF_FineTicketRecord> saved = LF_FineTickets.ExportAll();
		LF_FineTickets.ApplyAll(saved);
		LF_FineTickets.ApplyAll(saved);
		LF_FineTicketRecord reloaded = LF_FineTickets.Find(away.m_iNumber);
		ctx.True(reloaded && !reloaded.m_bPaid && reloaded.m_iAmount == 200, "an unpaid ticket survives export and apply");
		LF_FineTicketRecord receipt = LF_FineTickets.Find(ticket.m_iNumber);
		ctx.True(receipt && receipt.m_bPaid, "the paid receipt survives export and apply");
		LF_FineTicketRecord next = LF_FineTickets.Issue(OFFLINE, "Cy Away", OFFICER, "Bo Officer", LF_PoliceRanks.PATROL, "#LF-Ticket_OffenceSpeeding", 150, "08/11/1989 14:00");
		ctx.Equal(away.m_iNumber + 1, next.m_iNumber, "numbering continues after a load");

		LF_WantedReports.ClearFor(HOLDER);
		holder.SetWantedLevel(0);
		LF_FineTickets.ApplyAll(before);
		LF_FineTickets.UseConfig();
	}
}
