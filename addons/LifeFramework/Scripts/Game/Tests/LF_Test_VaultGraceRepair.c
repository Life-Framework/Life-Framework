// red-proof: Observed 2026-11-18 with `toolsi test --no-build --red LF_Test_VaultGraceRepair`: RED-OK, 7 assertions fail with the hook on (grace robbery and breach start, off-duty repair offered and done, vault table 1 after the skipped reset); green without it.
// tier: WORLD
//! Vault abuse gates (#2539): a character in its grace cannot start a vault robbery or arm a breach
//! (the charge stays in the pack's place: nothing arms); an off-duty police account cannot relock a
//! vault, an on-duty one can; the bank's world-end reset hook leaves no vault behind.
class LF_Test_VaultGraceRepair : LF_Test
{
	protected static const string UID_NEW = "vault-grace-new";
	protected static const string UID_POLICE = "vault-grace-police";
	protected static const string VAULT = "vault-grace-vault";
	protected static const string DOOR = "vault-grace-door";

	override string GetName()
	{
		return "crime/vault-grace-repair";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_GraceClockRow> clocksBefore = LF_GraceCharacterClock.ExportAll();
		array<ref LF_CrimeRecordRow> crimesBefore = LF_CrimeRecord.ExportAll();
		LF_GraceCharacterClock.ApplyAll(null);
		LF_CrimeRecord.ApplyAll(null);

		LF_Bank.Configure(VAULT, 1, 100, 0, 0, 0);
		LF_Breach.Configure(DOOR, 0, "");

		// Grace: ten minutes into a thirty minute grace.
		LF_GraceCharacterClock.Credit(UID_NEW, 600);
		LF_Grace.SetMinutesOverride(30);
		if (RedProof())
			LF_Grace.SetMinutesOverride(0);

		ctx.True(LF_Grace.Protects(UID_NEW) || RedProof(), "the new character is in its grace");
		ctx.False(LF_Bank.Rob(VAULT, UID_NEW), "a character in its grace cannot start a vault robbery");
		ctx.True(LF_Breach.IsLocked(VAULT), "the vault stays locked");
		ctx.False(LF_Breach.Arm(DOOR, UID_NEW), "a character in its grace cannot arm a breach");
		ctx.True(LF_Breach.IsLocked(DOOR), "the door stays locked");
		LF_Grace.SetMinutesOverride(-1);

		// Repair: only police on duty. Open the door as a character past its grace.
		LF_GraceCharacterClock.ApplyAll(null);
		LF_CrimeRecord.ApplyAll(null);
		LF_Breach.Reset(DOOR);
		ctx.True(LF_Breach.Arm(DOOR, UID_NEW), "out of grace the breach arms and opens");
		ctx.True(LF_Breach.IsOpen(DOOR), "the door is open");

		LF_PlayerAccount police = LF_PlayerAccountManager.GetOrCreate(UID_POLICE);
		police.SetFaction(LF_Faction.POLICE);
		police.SetOnDuty(RedProof());
		ctx.False(LF_Breach.CanRepair(DOOR, UID_POLICE), "an off-duty police account is not offered the repair");
		ctx.False(LF_Breach.Repair(DOOR, UID_POLICE), "an off-duty police account cannot repair");
		police.SetOnDuty(true);
		ctx.True(LF_Breach.Repair(DOOR, UID_POLICE), "an on-duty police account can repair");
		ctx.True(LF_Breach.IsLocked(DOOR), "the repaired door is locked");
		police.SetOnDuty(false);
		police.SetFaction(LF_Faction.CIVILIAN);

		// World end: the bank's own hook empties the vault table. The world's vaults go back after.
		ctx.True(LF_Bank.VaultCount() > 0, "vaults are configured before the reset");
		map<string, ref LF_BankVaultState> worldVaults = LF_Bank.SwapVaults(null);
		LF_Bank.Configure(VAULT, 1, 100, 0, 0, 0);
		if (!RedProof())
		{
			LF_StaticResetHook hook = new LF_Bank_StaticResetHook();
			hook.Apply();
		}

		ctx.Equal(0, LF_Bank.VaultCount(), "after the bank's reset hook the vault table is empty");
		LF_Bank.SwapVaults(worldVaults);
		if (worldVaults)
			worldVaults.Remove(VAULT);

		LF_Breach.Reset(DOOR);
		LF_Breach.Reset(VAULT);
		LF_GraceCharacterClock.ApplyAll(clocksBefore);
		LF_CrimeRecord.ApplyAll(crimesBefore);
	}
}
