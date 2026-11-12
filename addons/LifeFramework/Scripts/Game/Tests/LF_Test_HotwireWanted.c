// red-proof: 2026-11-10 `tools\cli test --no-build --only LF_Test_HotwireWanted` with ApplyTheft's IncreaseWantedLevel removed failed `- a successful hotwire raises the thief to wanted 1 (expected 1, got 0)`. Restored; green.
// tier: LOGIC
//! #495: a hotwired car puts the thief on the wanted board, so the entry must not read level 0.
class LF_Test_HotwireWanted : LF_Test
{
	override string GetName()
	{
		return "hotwire/theft-wanted";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PlayerAccount account = LF_PlayerAccount.Create("test-hotwire-wanted");
		ctx.Equal(0, LF_Hotwire.ApplyTheft(account, "test-hotwire-wanted", false), "a failed hotwire adds no wanted level");
		ctx.Equal(0, account.GetWantedLevel(), "a failed hotwire leaves wanted 0");
		ctx.Equal(1, LF_Hotwire.ApplyTheft(account, "test-hotwire-wanted", true), "a successful hotwire raises the thief to wanted 1");
		ctx.Equal(1, account.GetWantedLevel(), "the account holds wanted 1");
		ctx.Equal(0, LF_Hotwire.ApplyTheft(null, "", true), "no account is a no-op");
	}
}
