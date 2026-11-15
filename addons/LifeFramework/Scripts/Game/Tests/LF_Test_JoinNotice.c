// red-proof: Observed 2026-11-16: before the classes existed `tools\cli test --no-build --only LF_Test_JoinNotice` failed to compile ("Can't find variable 'LF_JoinNotice'"); green after. `--red LF_Test_JoinNotice` then RED-OK: skipping the spawn failed "the first spawn of a uid shows one toast (expected 1, got 0)".
// tier: LOGIC
//! The alpha notice at a player's first spawn (#1448): the `joinNotice` owner section (`enabled`,
//! `key`) shows one toast per account uid per session, driven by LF_OnPlayerSpawnedEvent. Enabled
//! with a key: one toast per uid, none on a respawn, one more for another uid. Disabled (the
//! default): none. An empty key: one LF_Debug.Warn for the session and no toast.
class LF_Test_JoinNotice : LF_Test
{
	static const string ON_FIXTURE = "{\"schemaVersion\":1,\"joinNotice\":{\"enabled\":true,\"key\":\"LF_Test_Notice\"}}";
	static const string OFF_FIXTURE = "{\"schemaVersion\":1,\"joinNotice\":{\"enabled\":false,\"key\":\"LF_Test_Notice\"}}";
	static const string EMPTY_KEY_FIXTURE = "{\"schemaVersion\":1,\"joinNotice\":{\"enabled\":true,\"key\":\"\"}}";

	override string GetName()
	{
		return "rules/join-notice";
	}

	//! Loads an owner config, installs the subscriber on a fresh bus and counts toasts instead of sending.
	protected void Arrange(string ownerJson, string source)
	{
		LF_EventBus.ResetStatics();
		LF_Session.ResetStatics();
		LF_JoinNotice.ResetStatics();
		LF_JoinNotice.CountOnly(true);
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString(ownerJson, source);
		LF_JoinNoticeConfig.Reload();
		LF_JoinNotice.Install();
	}

	override void Run(LF_TestContext ctx)
	{
		// No section: off by default, nothing is shown.
		Arrange("{\"schemaVersion\":1}", "test:joinnotice-none");
		ctx.True(!LF_JoinNoticeConfig.Get().m_bEnabled, "the notice is off by default");
		ctx.True(LF_ConfigService.IsKnownTopLevelKey(LF_JoinNoticeConfig.SECTION), "joinNotice is a known top-level key");
		LF_Session.OnPlayerSpawned(3, "uid-a");
		ctx.Equal(0, LF_JoinNotice.ToastCount(), "the default config shows no toast");

		// Disabled with a key: none.
		Arrange(OFF_FIXTURE, "test:joinnotice-off");
		LF_Session.OnPlayerSpawned(3, "uid-a");
		ctx.Equal(0, LF_JoinNotice.ToastCount(), "disabled shows no toast");

		// Enabled with a key: one toast per uid per session.
		Arrange(ON_FIXTURE, "test:joinnotice-on");
		ctx.True(LF_JoinNoticeConfig.Get().m_bEnabled, "enabled is read");
		ctx.EqualStr("LF_Test_Notice", LF_JoinNoticeConfig.Get().m_sKey, "the key is read");
		if (!RedProof())
			LF_Session.OnPlayerSpawned(3, "uid-a");

		ctx.Equal(1, LF_JoinNotice.ToastCount(), "the first spawn of a uid shows one toast");
		ctx.Equal(3, LF_JoinNotice.LastPlayerId(), "the toast goes to the spawning player");
		ctx.EqualStr("#LF_Test_Notice", LF_JoinNotice.LastMessage(), "the toast carries the configured key as a #key");

		LF_Session.OnPlayerSpawned(3, "uid-a");
		ctx.Equal(1, LF_JoinNotice.ToastCount(), "a respawn of the same uid shows no second toast");

		LF_Session.OnPlayerSpawned(4, "uid-b");
		ctx.Equal(2, LF_JoinNotice.ToastCount(), "another uid gets its own toast");
		ctx.Equal(4, LF_JoinNotice.LastPlayerId(), "the second toast goes to the second player");

		// A new session (world reset) tells the same uid again.
		LF_Session.ResetStatics();
		LF_JoinNotice.ResetSession();
		LF_Session.OnPlayerSpawned(3, "uid-a");
		ctx.Equal(3, LF_JoinNotice.ToastCount(), "a new session shows the notice again");

		// Enabled with an empty key: one Warn for the session, no toast.
		Arrange(EMPTY_KEY_FIXTURE, "test:joinnotice-emptykey");
		LF_Session.OnPlayerSpawned(3, "uid-a");
		LF_Session.OnPlayerSpawned(4, "uid-b");
		ctx.Equal(0, LF_JoinNotice.ToastCount(), "an empty key shows no toast");
		ctx.Equal(1, LF_JoinNotice.WarnCount(), "an empty key warns once, not per player");

		// Leave nothing behind for the next test.
		LF_JoinNotice.ResetStatics();
		LF_EventBus.ResetStatics();
		LF_Session.ResetStatics();
		LF_ConfigService.Reset();
		LF_JoinNoticeConfig.Reload();
	}
}
