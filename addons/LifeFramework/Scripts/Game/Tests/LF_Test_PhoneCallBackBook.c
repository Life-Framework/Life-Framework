// red-proof: Observed 2026-11-12 with `tools/cli test --no-build --only LF_Test_PhoneCallBackBook`: skipping the length check in LF_PhoneCallBackBook.Leave failed it with `- a message over the limit is refused`. Restored; green.
//------------------------------------------------------------------------------------------------
//! comms/phone-callback-book (#147): the call-back book alone. A message is cleaned of markup and
//! separators; a request needs a live offer at that phone and a short enough message; one open
//! request per caller and a bounded book; the pad lists it; a ring-back claims it, a pick-up closes
//! it, a ring-out reopens it; requests expire, never while being rung back.
// tier: LOGIC
class LF_Test_PhoneCallBackBook : LF_Test
{
	override string GetName()
	{
		return "comms/phone-callback-book";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.EqualStr("call me back b now /b", LF_PhoneCallBackBook.Clean("  call me | back <b>now</b>\n "), "Clean drops separators, markup and new lines, and trims");
		ctx.EqualStr("", LF_PhoneCallBackBook.Clean("  \t "), "Clean of blanks is empty");

		LF_PhoneCallBackBook book = new LF_PhoneCallBackBook();
		LF_EPhoneRequestRefusal refusal;
		ctx.True(book.Leave(10, "kim", "50,50", "hi", "12:00", 20, 900, 5, refusal) == null && refusal == LF_EPhoneRequestRefusal.NO_OFFER, "no missed call, no request");

		book.Offer(10, "kim", 1, "50,50", "police", "fuel-station", 30, 120);
		ctx.Equal(1, book.OfferedAt("50,50"), "the payphone holds Kim's offer");
		ctx.True(book.Leave(11, "kim", "60,60", "hi", "12:00", 20, 900, 5, refusal) == null && refusal == LF_EPhoneRequestRefusal.NO_OFFER, "the offer is for that phone only");
		ctx.True(book.Leave(11, "lee", "50,50", "hi", "12:00", 20, 900, 5, refusal) == null && refusal == LF_EPhoneRequestRefusal.NO_OFFER, "the offer is for that caller only");
		ctx.True(book.Leave(11, "kim", "50,50", "this message is far too long", "12:00", 20, 900, 5, refusal) == null && refusal == LF_EPhoneRequestRefusal.TOO_LONG, "a message over the limit is refused");
		ctx.True(book.Leave(130, "kim", "50,50", "hi", "12:00", 20, 900, 5, refusal) == null && refusal == LF_EPhoneRequestRefusal.NO_OFFER, "the offer runs out");

		book.Offer(200, "kim", 1, "50,50", "police", "fuel-station", 30, 120);
		LF_PhoneCallBackRequest first = book.Leave(201, "kim", "50,50", "robbery", "12:01", 20, 900, 5, refusal);
		ctx.True(first != null && refusal == LF_EPhoneRequestRefusal.OK, "Kim leaves a request");
		ctx.Equal(0, book.OfferedAt("50,50"), "leaving it uses up the offer");
		ctx.True(book.Leave(202, "kim", "50,50", "again", "12:01", 20, 900, 5, refusal) == null, "one offer, one request");

		book.Offer(210, "kim", 1, "50,50", "police", "fuel-station", 25, 120);
		LF_PhoneCallBackRequest second = book.Leave(211, "kim", "50,50", "", "12:02", 20, 900, 5, refusal);
		ctx.True(second != null, "an empty message is a request too");
		ctx.Equal(1, book.Requests().Count(), "a caller's new request replaces their open one");

		array<string> pad = {};
		ctx.Equal(1, book.Entries(212, "police", pad), "the police pad lists it");
		if (pad.Count() == 1)
			ctx.EqualStr(string.Format("%1|12:02|25|0|fuel-station|", second.m_iId), pad[0], "pad entry: id, time, rang, calling, place, message");
		ctx.Equal(0, book.Entries(212, "medic", pad), "another service's pad does not");

		// The book is bounded: the oldest request goes.
		for (int i = 0; i < 6; i++)
		{
			string uid = "caller" + i.ToString();
			book.Offer(300, uid, 10 + i, "7" + i.ToString() + ",70", "police", "", 30, 120);
			book.Leave(301, uid, "7" + i.ToString() + ",70", "", "12:03", 20, 900, 5, refusal);
		}
		ctx.Equal(5, book.Requests().Count(), "the book holds at most the configured requests");
		ctx.True(book.Find(second.m_iId) == null, "the oldest request went first");

		// Claim, then the ring-back's outcome.
		LF_PhoneCallBackRequest target = book.Requests()[0];
		ctx.True(book.Claimable(310, target.m_iId, "medic", refusal) == null && refusal == LF_EPhoneRequestRefusal.NOT_FOUND, "a medic cannot ring back a police request");
		ctx.True(book.Claimable(310, target.m_iId, "police", refusal) == target, "an officer can ring it back");
		target.m_iCallId = 42;
		ctx.True(book.Claimable(310, target.m_iId, "police", refusal) == null && refusal == LF_EPhoneRequestRefusal.CALLING, "a request being rung back cannot be claimed twice");
		book.Entries(310, "police", pad);
		ctx.True(pad.Count() > 0 && pad[0].Contains("|1|"), "the pad shows it as being rung back");
		ctx.True(book.OnCallBack(42, false, true) == target && target.m_iCallId == 0, "a ring-back nobody picked up reopens it");
		target.m_iCallId = 43;
		array<ref LF_PhoneCallBackRequest> expired = {};
		book.Purge(5000, expired);
		ctx.True(book.Find(target.m_iId) == target, "a request being rung back never expires");
		ctx.Equal(4, expired.Count(), "the others expired with the board");
		ctx.True(book.OnCallBack(43, true, false) == target && book.Find(target.m_iId) == null, "a picked-up ring-back closes the request");
	}
}
