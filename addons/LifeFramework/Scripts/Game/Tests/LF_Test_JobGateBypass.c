// red-proof: observed 2026-11-15 with `tools/cli test --no-build --red LF_Test_JobGateBypass`: RED PROOF OK, the RedProof() hook skips the gate-on switch and `gate on: the police job is not restricted`, `gate on: the police job whitelist passes` and `gate on: the replicated state offers Clock in` fail. Green on the real switch.
//------------------------------------------------------------------------------------------------
//! jobs/gate-bypass: the testing bypass for gated jobs (LF_JobGates). With the switch off a new
//! player off the police roster is refused at the duty desk; with it on the same player clocks in,
//! the whitelist reports nothing restricted and the replicated roster bit reads true. A dedicated
//! server refuses the bypass unless the mission header allows it. The owner config key
//! `jobs.testUnlockAll` is read and defaults to off.
// tier: WORLD
class LF_Test_JobGateBypass : LF_Test
{
	protected static const string UID = "job-gate-bypass-newcomer";

	override string GetName()
	{
		return "jobs/gate-bypass";
	}

	override void Run(LF_TestContext ctx)
	{
		// The pure rule: asked by config or header, never on a dedicated server without the header's allow.
		ctx.False(LF_JobGates.Resolve(false, false, false, false), "nothing asks for it: off");
		ctx.False(LF_JobGates.Resolve(false, false, true, true), "nothing asks for it, even with the dedicated allow: off");
		ctx.True(LF_JobGates.Resolve(true, false, false, false), "the owner config asks on a listen or Workbench host: on");
		ctx.True(LF_JobGates.Resolve(false, true, false, false), "the mission header asks on a listen or Workbench host: on");
		ctx.False(LF_JobGates.Resolve(true, false, true, false), "a dedicated server refuses the config switch without the header's allow");
		ctx.False(LF_JobGates.Resolve(false, true, true, false), "a dedicated server refuses the header switch without its own allow");
		ctx.True(LF_JobGates.Resolve(false, true, true, true), "a dedicated server runs it when the header allows it");

		// The shipped default is off, and the owner key reads.
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"jobs\":{}}", "test");
		LF_JobsConfig quiet = new LF_JobsConfig();
		quiet.ApplyLayers();
		ctx.False(quiet.TestUnlockAll(), "jobs.testUnlockAll is off when the owner leaves it out");
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"jobs\":{\"testUnlockAll\":true}}", "test");
		LF_JobsConfig loud = new LF_JobsConfig();
		loud.ApplyLayers();
		ctx.True(loud.TestUnlockAll(), "jobs.testUnlockAll: true reads");
		LF_ConfigService.Reset();

		// Gate off: a new player is refused.
		LF_JobGates.UseForTest(LF_JobGates.OVERRIDE_OFF);
		LF_PlayerAccount account = LF_TestPoliceKit.Account(UID, false);
		ctx.EqualStr("#LF-Duty_NotOnRoster", LF_DutyLocker.RefuseClockIn(account, LF_Faction.POLICE), "gate off: a new player cannot clock in as police");
		ctx.True(LF_WhitelistManager.IsFactionRestricted(LF_EFactionType.POLICE), "gate off: police is restricted");
		ctx.True(LF_WhitelistManager.IsJobRestricted(LF_EJobType.POLICE), "gate off: the police job is restricted");
		ctx.False(LF_WhitelistManager.IsWhitelistedForJob(UID, LF_EJobType.POLICE), "gate off: a new player is not whitelisted for the police job");
		ctx.False(LF_AccountStateBits.IsOnRoster(LF_AccountStateBits.FromAccount(account)), "gate off: the replicated state does not offer the roster");

		// Gate on: the same player is allowed.
		if (!RedProof())
			LF_JobGates.UseForTest(LF_JobGates.OVERRIDE_ON);
		ctx.EqualStr("", LF_DutyLocker.RefuseClockIn(account, LF_Faction.POLICE), "gate on: the same player clocks in as police");
		ctx.EqualStr("", LF_DutyLocker.RefuseClockIn(account, LF_Faction.MEDIC), "gate on: and as a medic");
		ctx.True(LF_WhitelistManager.MayJoinFaction(UID, LF_EFactionType.POLICE), "gate on: the police faction opens");
		ctx.False(LF_WhitelistManager.IsJobRestricted(LF_EJobType.POLICE), "gate on: the police job is not restricted");
		ctx.True(LF_WhitelistManager.IsWhitelistedForJob(UID, LF_EJobType.POLICE), "gate on: the police job whitelist passes");
		ctx.True(LF_AccountStateBits.IsOnRoster(LF_AccountStateBits.FromAccount(account)), "gate on: the replicated state offers Clock in");

		// An account already on duty is still refused: the bypass opens the roster, nothing else.
		account.SetOnDuty(true);
		ctx.EqualStr("#LF-Duty_AlreadyOn", LF_DutyLocker.RefuseClockIn(account, LF_Faction.POLICE), "gate on: a second clock-in is still refused");

		LF_JobGates.UseForTest(LF_JobGates.OVERRIDE_NONE);
		LF_TestPoliceKit.Release(UID);
	}
}
