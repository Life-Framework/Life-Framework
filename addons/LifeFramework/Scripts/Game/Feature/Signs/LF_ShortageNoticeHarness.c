// Harness verb `clinicshort on|off|state` (#4448): force the clinic shortage so a session proof can look at the
// door notice. `on` starts the shortage the way a robbed cabinet does (server), `off` ends it at once, `state`
// answers `short=<0|1>` from the state this client holds.

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "clinicshort")
			return super.Handle(cmd);

		if (cmd.Arg1 == "on")
		{
			LF_ClinicShortage.Start("harness-cabinet", "harness");
			LF_AgentHarness.Answer(cmd.Id, "ok", "clinic short");
			return true;
		}

		if (cmd.Arg1 == "off")
		{
			LF_ClinicShortage.s_iNowOverride = LF_ClinicShortage.Now() + 100000000;
			LF_ClinicShortage.Expire();
			LF_ClinicShortage.s_iNowOverride = -1;
			LF_AgentHarness.Answer(cmd.Id, "ok", "clinic stocked");
			return true;
		}

		if (cmd.Arg1 == "state")
		{
			int isShort;
			if (LF_ShortageNotice.IsShort())
				isShort = 1;

			LF_AgentHarness.Answer(cmd.Id, "ok", string.Format("short=%1", isShort));
			return true;
		}

		LF_AgentHarness.Answer(cmd.Id, "error", "usage: clinicshort on|off|state");
		return true;
	}
}
