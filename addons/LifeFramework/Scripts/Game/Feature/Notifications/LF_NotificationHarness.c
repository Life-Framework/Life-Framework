// Harness verb `toast <info|success|warning|error|police|ems|robbery|sms> [text]` (client side), for the session
// picture of a toast the sender gave no title. It shows one slip of that kind on THIS client with an empty title,
// as a refusal or a pickup toast does, so a proof can look at the fallback title (none for info, success, warning
// and error; plain words for an alert kind). The text is one word of the sent line. Local only, no state.
// Replies `ok toast=<kind>` or an error.

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "toast")
			return super.Handle(cmd);

		LF_ENotificationType kind = LF_ENotificationType.INFO;
		if (cmd.Arg1 == "success")
			kind = LF_ENotificationType.SUCCESS;
		else if (cmd.Arg1 == "warning")
			kind = LF_ENotificationType.WARNING;
		else if (cmd.Arg1 == "error")
			kind = LF_ENotificationType.ERROR;
		else if (cmd.Arg1 == "police")
			kind = LF_ENotificationType.POLICE_ALERT;
		else if (cmd.Arg1 == "ems")
			kind = LF_ENotificationType.EMS_ALERT;
		else if (cmd.Arg1 == "robbery")
			kind = LF_ENotificationType.ROBBERY_ALERT;
		else if (cmd.Arg1 == "sms")
			kind = LF_ENotificationType.SMS_RECEIVED;
		else if (!cmd.Arg1.IsEmpty() && cmd.Arg1 != "info")
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "unknown kind " + cmd.Arg1);
			return true;
		}

		LF_NotificationManagerComponent manager = LF_NotificationManagerComponent.GetInstance();
		if (!manager)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no notification manager");
			return true;
		}

		string text = cmd.Arg2;
		if (text.IsEmpty())
			text = "Iron Bar x5 The trader pays $20 for Iron Bar.";

		manager.ShowNotificationToast(string.Empty, text, 20, kind);
		LF_AgentHarness.Answer(cmd.Id, "ok", "toast=" + cmd.Arg1);
		return true;
	}
}
