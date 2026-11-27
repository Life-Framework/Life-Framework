// Harness verb `activities state|pick <n> [direct]|go [direct]` (the "What to do next" tab, client side; open
// the tab with `playermenu open next`). `pick <n>` clicks the n-th row's Read button as a player does (a row
// with no visible button answers `widget_not_found PlayerMenuNext_<n>_Action`; #5099), `go` clicks the START /
// GUIDE ME button (the server decides); `direct` skips the widget and calls the handler. `state` answers
// `rows=<n> picked=<n> ids=<id:state,...>` (state: 1 available, 2 running, 3 done) so a proof waits.

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "activities")
			return super.Handle(cmd);

		LF_UIScreen screen = LF_PlayerMenuSheet.Current();
		if (cmd.Arg1 == "pick" || cmd.Arg1 == "go")
		{
			if (!screen || !screen.IsOpen() || LF_PlayerMenuSheet.CurrentTab() != LF_PlayerMenuSheet.TAB_NEXT)
			{
				LF_AgentHarness.Answer(cmd.Id, "error", "the next tab is not open");
				return true;
			}

			string part = LF_ActivityPage.PART_GO;
			string flag = cmd.Arg2;
			if (cmd.Arg1 == "pick")
			{
				part = LF_ActivityPage.ROW + cmd.Arg2.ToInt().ToString();
				flag = cmd.Arg3;
			}

			// A player presses the row's Read button, so the default presses that widget; a row with no
			// visible button answers `widget_not_found <name>`. `direct` calls the handler (no widget).
			if (flag == "direct" || flag == "--direct")
			{
				screen.Notify(part);
				LF_AgentHarness.Answer(cmd.Id, "ok", "pressed " + part + " (direct)");
				return true;
			}

			string widgetName = part;
			if (cmd.Arg1 == "pick")
				widgetName = part + "_Action";

			string error = LF_AgentHarness.PressWidget(widgetName, 0);
			if (!error.IsEmpty())
			{
				LF_AgentHarness.Answer(cmd.Id, "error", error);
				return true;
			}

			LF_AgentHarness.Answer(cmd.Id, "ok", "pressed " + part + " (ui)");
			return true;
		}

		if (cmd.Arg1 == "state")
		{
			LF_ActivityData data = LF_ActivityPage.Data();
			string ids = string.Empty;
			int rows = 0;
			if (data)
			{
				rows = data.m_aRows.Count();
				foreach (LF_ActivityRow row : data.m_aRows)
				{
					if (!ids.IsEmpty())
						ids += ",";

					ids += row.m_sId + ":" + row.m_eState.ToString();
				}
			}

			LF_AgentHarness.Answer(cmd.Id, "ok", string.Format("rows=%1 picked=%2 ids=%3", rows, LF_ActivityPage.Picked(), ids));
			return true;
		}

		LF_AgentHarness.Answer(cmd.Id, "error", "usage: activities state|pick <n> [direct]|go [direct]");
		return true;
	}
}
