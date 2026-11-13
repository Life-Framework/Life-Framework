// red-proof: Observed 2026-11-12 with `tools/cli test --no-build --only LF_Test_PhoneExchange`: skipping the busy-phone check (`CallAtPhone(fromPhone)`) in LF_PhoneExchange.CheckDial failed it with `- a payphone in use cannot dial`. Restored; green.
//------------------------------------------------------------------------------------------------
//! comms/phone-exchange (#147): the call table alone. A dial rings every idle station phone; a
//! second dial gets the busy signal; only an allowed player picks up; the line states follow the
//! call; hang-up is the talker's own; a call rings out as MISSED and a long one is cut; a call
//! nobody could take is MISSED from the start and holds no phone.
// tier: LOGIC
class LF_Test_PhoneExchange : LF_Test
{
	override string GetName()
	{
		return "comms/phone-exchange";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PhoneExchange exchange = new LF_PhoneExchange();
		array<string> station = {"10,10", "12,10"};
		LF_EPhoneRefusal refusal;

		// Dial: rings both station phones, the payphone is busy with the caller on it.
		LF_PhoneCall call = exchange.Dial(100, 1, "caller", "50,50", "police", station, false, refusal);
		ctx.True(call != null, "the first dial rings");
		ctx.True(refusal == LF_EPhoneRefusal.OK, "the first dial is not refused");
		if (!call)
			return;

		ctx.True(call.m_eState == LF_EPhoneCallState.RINGING, "a new call is RINGING");
		ctx.Equal(2, call.m_aRingPhones.Count(), "it rings every idle station phone");
		int talker;
		ctx.True(exchange.LineState("10,10", talker) == LF_EPhoneLineState.RINGING, "the station phone shows RINGING");
		ctx.Equal(0, talker, "nobody talks at a ringing phone");
		ctx.True(exchange.LineState("50,50", talker) == LF_EPhoneLineState.BUSY, "the payphone is off the hook");
		ctx.Equal(1, talker, "the caller talks at the payphone");

		// While it rings: the caller cannot dial again, the station is engaged for a second caller.
		ctx.True(exchange.Dial(101, 1, "caller", "60,60", "police", station, false, refusal) == null && refusal == LF_EPhoneRefusal.ON_A_CALL, "a caller on a call cannot dial again");
		ctx.True(exchange.Dial(101, 2, "second", "60,60", "police", station, false, refusal) == null && refusal == LF_EPhoneRefusal.LINE_BUSY, "a second caller gets the busy signal");
		ctx.True(exchange.Dial(101, 2, "second", "50,50", "police", station, false, refusal) == null && refusal == LF_EPhoneRefusal.PHONE_BUSY, "a payphone in use cannot dial");
		array<string> none = {};
		ctx.True(exchange.Dial(101, 2, "second", "60,60", "police", none, false, refusal) == null && refusal == LF_EPhoneRefusal.NO_LINE, "a service with no station phone has no line");

		// Pick-up: refused when not ringing or not allowed; then the first allowed one connects.
		ctx.True(exchange.Answer(105, 3, "officer", "99,99", true, refusal) == null && refusal == LF_EPhoneRefusal.NOT_RINGING, "a phone with no call is not ringing");
		ctx.True(exchange.Answer(105, 4, "civilian", "10,10", false, refusal) == null && refusal == LF_EPhoneRefusal.NOT_ON_DUTY, "a player who may not answer is refused");
		ctx.True(exchange.Answer(105, 1, "caller", "10,10", true, refusal) == null && refusal == LF_EPhoneRefusal.ON_A_CALL, "the caller cannot pick up their own call");
		LF_PhoneCall answered = exchange.Answer(106, 3, "officer", "12,10", true, refusal);
		ctx.True(answered == call, "the officer picks up the second station phone");
		ctx.True(call.m_eState == LF_EPhoneCallState.CONNECTED, "the call is CONNECTED");
		ctx.Equal(6, call.RangSeconds(), "it rang for 6 s");
		ctx.True(exchange.LineState("12,10", talker) == LF_EPhoneLineState.BUSY && talker == 3, "the officer talks at the phone they picked up");
		ctx.True(exchange.LineState("10,10", talker) == LF_EPhoneLineState.IDLE, "the other station phone stops ringing");
		ctx.True(exchange.Answer(107, 5, "officer2", "10,10", true, refusal) == null && refusal == LF_EPhoneRefusal.NOT_RINGING, "an answered call cannot be picked up twice");

		// Hang-up: only a talker, only at their own phone.
		ctx.True(exchange.HangUp(110, 4, "50,50", refusal) == null && refusal == LF_EPhoneRefusal.NOT_YOUR_CALL, "a bystander cannot hang up a call");
		ctx.True(exchange.HangUp(110, 3, "50,50", refusal) == null && refusal == LF_EPhoneRefusal.NOT_YOUR_CALL, "a talker hangs up at their own phone only");
		ctx.True(exchange.HangUp(120, 3, "12,10", refusal) == call, "the officer hangs up");
		ctx.True(call.m_eState == LF_EPhoneCallState.ENDED && call.m_eEnd == LF_EPhoneEnd.HUNG_UP, "a hung-up call is ENDED, HUNG_UP");
		ctx.Equal(14, call.TalkSeconds(), "they talked 14 s");
		ctx.True(exchange.LineState("50,50", talker) == LF_EPhoneLineState.IDLE && exchange.LineState("12,10", talker) == LF_EPhoneLineState.IDLE, "both phones are idle again");
		ctx.True(!exchange.Finish(call, 121, LF_EPhoneEnd.DISCONNECTED), "an ended call does not end twice");

		// Rings out: MISSED after the ring time, ENDED for a call that ran too long.
		LF_PhoneCall unanswered = exchange.Dial(200, 2, "second", "60,60", "police", station, false, refusal);
		array<string> hospital = {"30,30"};
		LF_PhoneCall talking = exchange.Dial(200, 6, "third", "70,70", "medic", hospital, false, refusal);
		ctx.True(unanswered != null && talking != null, "two calls ring at once on different services");
		if (!unanswered || !talking)
			return;

		exchange.Answer(201, 7, "medic", "30,30", true, refusal);
		array<LF_PhoneCall> changed = {};
		exchange.Expire(229, 30, 60, changed);
		ctx.Equal(0, changed.Count(), "nothing expires before its time");
		exchange.Expire(230, 30, 60, changed);
		ctx.Equal(1, changed.Count(), "the unanswered call rings out at 30 s");
		ctx.True(unanswered.m_eState == LF_EPhoneCallState.MISSED && unanswered.m_eEnd == LF_EPhoneEnd.NO_ANSWER, "a rung-out call is MISSED, NO_ANSWER");
		exchange.Expire(261, 30, 60, changed);
		ctx.True(changed.Count() == 1 && talking.m_eState == LF_EPhoneCallState.ENDED && talking.m_eEnd == LF_EPhoneEnd.TOO_LONG, "a call past the maximum length is cut, TOO_LONG");

		// A call ending while it rings for any other reason is ENDED, not MISSED.
		LF_PhoneCall abandoned = exchange.Dial(300, 2, "second", "60,60", "police", station, false, refusal);
		ctx.True(abandoned != null && exchange.Finish(abandoned, 302, LF_EPhoneEnd.WALKED_AWAY) && abandoned.m_eState == LF_EPhoneCallState.ENDED, "a caller who walks off a ringing call ends it");

		// Nobody on duty: MISSED at once, nothing rings, the phone is free.
		LF_PhoneCall missed = exchange.Missed(400, 2, "second", "60,60", "police");
		ctx.True(missed.m_eState == LF_EPhoneCallState.MISSED && missed.m_eEnd == LF_EPhoneEnd.NOBODY_ON_DUTY, "a call nobody could take is MISSED, NOBODY_ON_DUTY");
		ctx.True(exchange.LineState("60,60", talker) == LF_EPhoneLineState.IDLE, "it holds no phone");

		// The table is bounded: finished calls beyond KEEP_FINISHED are dropped, live ones kept.
		LF_PhoneCall live = exchange.Dial(500, 9, "nine", "90,90", "police", station, false, refusal);
		for (int i = 0; i < LF_PhoneExchange.KEEP_FINISHED + 5; i++)
		{
			exchange.Missed(501 + i, 10, "ten", "91,91", "taxi");
		}
		exchange.Missed(600, 10, "ten", "91,91", "taxi");
		int finished;
		foreach (LF_PhoneCall any : exchange.Calls())
		{
			if (!any.IsLive())
				finished++;
		}
		ctx.True(finished <= LF_PhoneExchange.KEEP_FINISHED + 1, "finished calls stay bounded");
		ctx.True(exchange.Find(live.m_iId) == live, "a live call is never dropped");
	}
}
