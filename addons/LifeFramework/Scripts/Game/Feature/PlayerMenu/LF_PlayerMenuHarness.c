// Harness verb `playermenu open [person|progress|next]|pick <row>|close|state` (the personal file, client
// side). `open` shows the file on a tab the way the F2 key does and asks the server; `pick <n>` reads
// the n-th skill (person tab) or quest (progress tab) as its Read button does; `state` answers
// `open=<0|1> tab=<n> data=<0|1> skills=<n> quests=<n> licences=<n>` so a proof waits for the answer.

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "playermenu")
			return super.Handle(cmd);

		if (cmd.Arg1 == "open")
		{
			int tab = LF_PlayerMenuSheet.TAB_PERSON;
			if (cmd.Arg2 == "progress")
				tab = LF_PlayerMenuSheet.TAB_PROGRESS;
			else if (cmd.Arg2 == "next")
				tab = LF_PlayerMenuSheet.TAB_NEXT;

			if (!LF_PlayerMenuSheet.Open(tab))
			{
				LF_AgentHarness.Answer(cmd.Id, "error", "the file did not open");
				return true;
			}

			LF_AgentHarness.Answer(cmd.Id, "ok", "tab=" + LF_PlayerMenuSheet.CurrentTab().ToString());
			return true;
		}

		if (cmd.Arg1 == "pick")
		{
			LF_UIScreen screen = LF_PlayerMenuSheet.Current();
			LF_PlayerMenuData data = LF_PlayerMenuSheet.Data();
			if (!screen || !screen.IsOpen() || !data)
			{
				LF_AgentHarness.Answer(cmd.Id, "error", "the file is not open or has no data yet");
				return true;
			}

			string row = LF_PlayerMenuSheet.ROW_SKILL;
			if (LF_PlayerMenuSheet.CurrentTab() == LF_PlayerMenuSheet.TAB_PROGRESS)
				row = LF_PlayerMenuSheet.ROW_QUEST;

			screen.Notify(row + cmd.Arg2.ToInt().ToString());
			LF_AgentHarness.Answer(cmd.Id, "ok", "picked " + row + cmd.Arg2);
			return true;
		}

		if (cmd.Arg1 == "close")
		{
			LF_PlayerMenuSheet.Hide();
			LF_AgentHarness.Answer(cmd.Id, "ok", "closed");
			return true;
		}

		if (cmd.Arg1 == "state")
		{
			int open = 0;
			if (LF_PlayerMenuSheet.IsOpen())
				open = 1;

			LF_PlayerMenuData state = LF_PlayerMenuSheet.Data();
			int hasData = 0;
			int skills = 0;
			int quests = 0;
			int licences = 0;
			if (state)
			{
				hasData = 1;
				skills = state.m_aSkills.Count();
				quests = state.m_aQuests.Count();
				licences = state.m_aLicenceKeys.Count();
			}

			LF_AgentHarness.Answer(cmd.Id, "ok", string.Format("open=%1 tab=%2 data=%3 skills=%4 quests=%5 licences=%6", open, LF_PlayerMenuSheet.CurrentTab(), hasData, skills, quests, licences));
			return true;
		}

		LF_AgentHarness.Answer(cmd.Id, "error", "usage: playermenu open [person|progress|next]|pick <n>|close|state");
		return true;
	}
}
