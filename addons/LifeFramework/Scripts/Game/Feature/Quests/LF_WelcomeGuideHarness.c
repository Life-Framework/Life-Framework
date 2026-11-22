// Harness verb `guide open [page]|close|state` (welcome guide, client side). `open` shows the guide on a
// page (1 to 5, default 1) the way the key does; `close` puts it away; `state` answers `open=<0|1>
// page=<n> pages=<n>` so a proof can tell an auto-open happened. Buttons are pressed with `click`.

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "guide")
			return super.Handle(cmd);

		if (cmd.Arg1 == "open")
		{
			int page = 1;
			if (!cmd.Arg2.IsEmpty())
				page = cmd.Arg2.ToInt();

			if (!LF_WelcomeGuideSheet.Open(page - 1))
			{
				LF_AgentHarness.Answer(cmd.Id, "error", "the guide did not open");
				return true;
			}

			LF_AgentHarness.Answer(cmd.Id, "ok", "page=" + (LF_WelcomeGuideSheet.CurrentPage() + 1).ToString());
			return true;
		}

		if (cmd.Arg1 == "close")
		{
			LF_WelcomeGuideSheet.Hide();
			LF_AgentHarness.Answer(cmd.Id, "ok", "closed");
			return true;
		}

		if (cmd.Arg1 == "state")
		{
			LF_UIScreen screen = LF_WelcomeGuideSheet.Current();
			int open = 0;
			if (screen && screen.IsOpen())
				open = 1;

			LF_AgentHarness.Answer(cmd.Id, "ok", string.Format("open=%1 page=%2 pages=%3", open, LF_WelcomeGuideSheet.CurrentPage() + 1, LF_WelcomeGuideSheet.PageCount()));
			return true;
		}

		LF_AgentHarness.Answer(cmd.Id, "error", "usage: guide open [page]|close|state");
		return true;
	}
}
