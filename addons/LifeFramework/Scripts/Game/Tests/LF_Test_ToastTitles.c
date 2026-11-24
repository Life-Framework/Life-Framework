// red-proof: 2026-11-25, cli test --no-build --only LF_Test_ToastTitles on the unfixed code (the title key function still returned the [INFO]/[ERROR] keys): "[LFTEST] FAIL notifications/toast-titles - a titleless toast of kind ERROR has no title row, got: the old ERROR title key"; green after the fix.
//------------------------------------------------------------------------------------------------
//! notifications/toast-titles (2026-11-25 polish, round 2 "the toast carries the title [INFO]"): a toast the
//! sender gave no title shows no debug-style title. An info, success, warning or error slip has no title row at
//! all (the colour strip carries the kind); an alert kind (police, EMS, robbery, text message) shows plain words
//! with no bracket. Pure script: no widgets.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ToastTitles : LF_Test
{
	override string GetName()
	{
		return "notifications/toast-titles";
	}

	override void Run(LF_TestContext ctx)
	{
		array<LF_ENotificationType> untitled = {
			LF_ENotificationType.INFO,
			LF_ENotificationType.SUCCESS,
			LF_ENotificationType.WARNING,
			LF_ENotificationType.ERROR,
			LF_ENotificationType.CUSTOM
		};
		foreach (LF_ENotificationType kind : untitled)
		{
			string fallback = LF_NotificationManagerComponent.FallbackTitleKey(kind);
			ctx.True(fallback.IsEmpty(), "a titleless toast of kind " + typename.EnumToString(LF_ENotificationType, kind) + " has no title row, got: " + fallback);
		}

		array<LF_ENotificationType> alerts = {
			LF_ENotificationType.POLICE_ALERT,
			LF_ENotificationType.EMS_ALERT,
			LF_ENotificationType.ROBBERY_ALERT,
			LF_ENotificationType.SMS_RECEIVED
		};
		foreach (LF_ENotificationType alert : alerts)
		{
			string name = typename.EnumToString(LF_ENotificationType, alert);
			string key = LF_NotificationManagerComponent.FallbackTitleKey(alert);
			ctx.False(key.IsEmpty(), name + " keeps a title key");
			string words = LF_Terms.Text(key);
			ctx.False(words.IsEmpty(), name + " title resolves to words");
			ctx.Equal(-1, words.IndexOf("["), name + " title has no opening bracket: " + words);
			ctx.Equal(-1, words.IndexOf("]"), name + " title has no closing bracket: " + words);
		}
	}
}
