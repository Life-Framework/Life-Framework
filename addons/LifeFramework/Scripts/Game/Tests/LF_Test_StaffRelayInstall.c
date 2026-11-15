// red-proof: RED-OK moderation/staff-relay-install (fails with the hook on, 3 assertions: the relay is not installed after server start, admin-action subscribers expected 2 got 1, report-flag subscribers expected 1 got 0)
// tier: LOGIC
//! moderation/staff-relay-install (#1284): the game mode's OnGameStart calls LF_StaffRelay.Install.
//! On a server it takes effect once however often it is called (one subscriber per event, one
//! tick); on a client it does nothing; the world-end reset (LF_StaticReset -> Uninstall) removes
//! it and a later world can install again.
class LF_Test_StaffRelayInstall : LF_Test
{
	override string GetName()
	{
		return "moderation/staff-relay-install";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_StaffRelay.Uninstall();
		int adminBase = LF_EventBus.SubscriberCount(LF_OnAdminAction);
		int flagBase = LF_EventBus.SubscriberCount(LF_OnIncidentFlagged);

		// A client: no subscription, no tick.
		ctx.False(LF_StaffRelay.InstallAs(false), "a client does not install the relay");
		ctx.Equal(0, LF_StaffRelay.GetInstallCount(), "a client install takes no effect");
		ctx.False(LF_StaffRelay.IsInstalled(), "a client has no relay running");
		ctx.Equal(adminBase, LF_EventBus.SubscriberCount(LF_OnAdminAction), "a client adds no admin-action subscriber");
		ctx.Equal(flagBase, LF_EventBus.SubscriberCount(LF_OnIncidentFlagged), "a client adds no report-flag subscriber");

		// The server start: Install runs once, and again is a no-op.
		if (!RedProof())
			LF_StaffRelay.InstallAs(true);

		ctx.Equal(1, LF_StaffRelay.GetInstallCount(), "server start installs the relay once");
		ctx.True(LF_StaffRelay.IsInstalled(), "the relay runs after server start");
		ctx.Equal(adminBase + 1, LF_EventBus.SubscriberCount(LF_OnAdminAction), "one admin-action subscriber");
		ctx.Equal(flagBase + 1, LF_EventBus.SubscriberCount(LF_OnIncidentFlagged), "one report-flag subscriber");

		LF_StaffRelay.InstallAs(true);
		LF_StaffRelay.InstallAs(true);
		ctx.Equal(1, LF_StaffRelay.GetInstallCount(), "installing again takes no further effect");
		ctx.Equal(adminBase + 1, LF_EventBus.SubscriberCount(LF_OnAdminAction), "still one admin-action subscriber");
		ctx.Equal(flagBase + 1, LF_EventBus.SubscriberCount(LF_OnIncidentFlagged), "still one report-flag subscriber");

		// A client call on a running server relay changes nothing.
		ctx.False(LF_StaffRelay.InstallAs(false), "a client call still reports not installed");
		ctx.True(LF_StaffRelay.IsInstalled(), "a client call leaves the server relay alone");

		// World end: the reset removes it, a new world installs again.
		LF_StaffRelay.Uninstall();
		ctx.False(LF_StaffRelay.IsInstalled(), "the world-end reset stops the relay");
		ctx.Equal(adminBase, LF_EventBus.SubscriberCount(LF_OnAdminAction), "the reset drops the admin-action subscriber");
		ctx.Equal(flagBase, LF_EventBus.SubscriberCount(LF_OnIncidentFlagged), "the reset drops the report-flag subscriber");
		LF_StaffRelay.Uninstall();

		ctx.True(LF_StaffRelay.InstallAs(true), "the next world installs again");
		ctx.Equal(1, LF_StaffRelay.GetInstallCount(), "one install in the next world");
		LF_StaffRelay.Uninstall();
	}
}
