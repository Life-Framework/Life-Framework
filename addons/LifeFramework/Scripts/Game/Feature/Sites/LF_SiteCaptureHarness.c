//------------------------------------------------------------------------------------------------
//! Harness verb `sitebar` (#2686): puts the capture bar and an alert toast on THIS client's screen
//! so a session picture can show them without a staged gang war (agent-box/proofs/site-capture-bar-proof.mjs).
//!   sitebar show <hideoutId> <percent>   draws the bar (holder and challenger names are demo values)
//!   sitebar alert                        shows the "your ground is contested" toast
//!   sitebar hide                         takes the bar down
//!   sitebar state                        what the bar shows now
//! It calls the client half only (LF_SiteCaptureHud.Show/Hide, the toast); the server push and the
//! owner RPC are covered by the PR's manual steps. Dev gate: the harness is only in agent-box builds.
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "sitebar")
			return super.Handle(cmd);

		if (cmd.Arg1 == "show")
		{
			int percent = cmd.Arg3.ToInt();
			LF_SiteCaptureHud.Show(cmd.Arg2, "Harbour Syndicate", "Quarry Crew", percent);
		}
		else if (cmd.Arg1 == "hide")
		{
			LF_SiteCaptureHud.Hide();
		}
		else if (cmd.Arg1 == "alert")
		{
			LF_NotificationManagerComponent manager = LF_NotificationManagerComponent.GetInstance();
			if (manager)
				manager.ShowNotificationToast("#LF-SiteAlert_Title", LF_SiteAlerts.KEY_CONTESTED, LF_SiteAlerts.TOAST_SECONDS, LF_ENotificationType.WARNING);
		}

		LF_AgentHarness.Answer(cmd.Id, "ok", LF_SiteCaptureHud.Describe());
		return true;
	}
}
