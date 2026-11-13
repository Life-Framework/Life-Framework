// red-proof: Observed 2026-11-09 with `tools/cli test --no-build --only LF_Test_Payphone`: commenting out the player cooldown stamp (`s_mPlayerLast.Set(uid, now)`) in LF_Payphone.UseAt failed it with `- the same player is refused inside the cooldown`. Restored; green. Observed 2026-11-13: before the map.Find fix in UseAt it failed with `- a first call in the server's first minute connects` (a missing entry read as a call at time 0).
//------------------------------------------------------------------------------------------------
//! comms/payphone (#247): a call charges the coin once and reaches dispatch once; the cooldown
//! refuses a second call and takes no coin; a call nobody can answer takes no coin.
// tier: WORLD
class LF_Test_Payphone : LF_Test
{
	override string GetName()
	{
		return "comms/payphone";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_EventBus.ResetStatics();
		LF_Dispatch.ResetStatics();
		LF_Payphone.ResetStatics();
		LF_ServiceCallDispatch.ResetStatics();
		LF_ServiceCallDispatch.Install();

		IEntity caller = LF_TestCharacterFixture.Spawn(ctx);
		if (!caller)
			return;

		ctx.Equal(5, LF_MoneyUtils.AddCash(caller, 5), "seed 5 cash");
		int cost = LF_ServiceCallsConfig.PayphoneCost();
		ctx.True(cost > 0, "a call costs a coin");

		array<ref LF_ServiceCandidate> officer = {};
		officer.Insert(LF_ServiceCandidate.Make(1, true, LF_EJobType.UNEMPLOYED));
		vector phone = "96.5 0 263";
		string summary = "#LF-Payphone_Summary_Crime";
		array<string> board = {};

		// A server's first minute: a player who never called is not cooling down (a missing map
		// entry must not read as a call at time 0).
		ctx.True(LF_Payphone.UseAt(5, "payphone-early", caller, "50 0 50", "police", summary, officer) == LF_EPayphoneResult.CONNECTED, "a first call in the server's first minute connects");
		ctx.Equal(5 - cost, LF_MoneyUtils.GetCash(caller), "the early call took the coin");
		LF_MoneyUtils.AddCash(caller, cost);
		LF_Dispatch.ResetStatics();
		ctx.True(LF_Payphone.UseAt(100, "payphone-a", caller, phone, "police", summary, officer) == LF_EPayphoneResult.CONNECTED, "the first call connects");
		ctx.Equal(5 - cost, LF_MoneyUtils.GetCash(caller), "the call took the coin once");
		LF_Dispatch.BuildBoardEntries(10, board);
		ctx.Equal(1, board.Count(), "the call reached dispatch once");

		ctx.True(LF_Payphone.UseAt(101, "payphone-a", caller, phone, "police", summary, officer) == LF_EPayphoneResult.PLAYER_COOLDOWN, "the same player is refused inside the cooldown");
		ctx.True(LF_Payphone.UseAt(101, "payphone-b", caller, phone, "police", summary, officer) == LF_EPayphoneResult.PHONE_COOLDOWN, "another player is refused at the same phone inside its cooldown");
		ctx.Equal(5 - cost, LF_MoneyUtils.GetCash(caller), "refused calls take no coin");
		board.Clear();
		LF_Dispatch.BuildBoardEntries(10, board);
		ctx.Equal(1, board.Count(), "refused calls reach no dispatch");

		array<ref LF_ServiceCandidate> nobody = {};
		ctx.True(LF_Payphone.UseAt(101, "payphone-c", caller, "10 0 10", "police", summary, nobody) == LF_EPayphoneResult.NO_ANSWER, "nobody on duty gives no answer");
		ctx.Equal(5 - cost, LF_MoneyUtils.GetCash(caller), "an unanswered call takes no coin");

		ctx.True(LF_Payphone.UseAt(1000, "payphone-a", caller, phone, "police", summary, officer) == LF_EPayphoneResult.CONNECTED, "after the cooldown the call connects again");
		ctx.Equal(5 - 2 * cost, LF_MoneyUtils.GetCash(caller), "the second call took one more coin");

		LF_TestCharacterFixture.Delete(caller);
	}
}
