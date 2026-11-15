// red-proof: observed 2026-11-15: with the test and no LF_RestartToast.c, `tools\cli test --no-build --only LF_Test_RestartToast` failed (scripts do not compile); with the toast, green; `--red` run records the RED-OK line in the PR.

//------------------------------------------------------------------------------------------------
//! Captures what the toast would have sent to every player.
class LF_TestToastSender : LF_RestartToastSender
{
	int m_iSent;
	string m_sTitle;
	string m_sMessage;
	string m_sArg;

	override void Send(string title, string message, string messageArg)
	{
		m_iSent++;
		m_sTitle = title;
		m_sMessage = message;
		m_sArg = messageArg;
	}
}

// tier: LOGIC
//! O6 (#1265): every LF_OnRestartWarning on the server bus becomes one toast with the minutes
//! left in it; a repeat of the same lead inside the same minute sends nothing.
class LF_Test_RestartToast : LF_Test
{
	override string GetName()
	{
		return "ops/restart-toast";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_EventBus.ResetStatics();
		LF_RestartToast.ResetStatics();

		LF_TestToastSender sender = new LF_TestToastSender();
		LF_RestartToast.SetSender(sender);
		LF_RestartToast.SetClockMinute(1000);
		ctx.True(LF_RestartToast.Subscribe(), "the toast subscribed to the restart warning");

		// The red-proof hook skips the publish: no toast is sent and the checks below fail.
		if (!RedProof())
			LF_EventBus.Publish(LF_OnRestartWarning.Create(15, 900));

		ctx.Equal(1, sender.m_iSent, "one warning sent one toast");
		ctx.EqualStr("15", sender.m_sArg, "the toast carries the minutes left");
		ctx.EqualStr("#LF-Ops_RestartToast", sender.m_sMessage, "the toast text is the localization key");
		ctx.EqualStr("#LF-Ops_RestartTitle", sender.m_sTitle, "the toast title is the localization key");

		// The same lead in the same minute (a ticker re-fire) sends nothing.
		LF_EventBus.Publish(LF_OnRestartWarning.Create(15, 899));
		ctx.Equal(1, sender.m_iSent, "a second warning for the same lead in the same minute sent nothing");

		// A different lead is a new warning step.
		LF_EventBus.Publish(LF_OnRestartWarning.Create(5, 300));
		ctx.Equal(2, sender.m_iSent, "the next warning step sent its own toast");
		ctx.EqualStr("5", sender.m_sArg, "the second toast carries its own minutes");

		// The same lead a minute later is a fresh warning (the next restart cycle).
		LF_RestartToast.SetClockMinute(1001);
		LF_EventBus.Publish(LF_OnRestartWarning.Create(5, 300));
		ctx.Equal(3, sender.m_iSent, "the same lead in a later minute sent again");

		LF_RestartToast.ResetStatics();
		LF_EventBus.ResetStatics();
	}
}
