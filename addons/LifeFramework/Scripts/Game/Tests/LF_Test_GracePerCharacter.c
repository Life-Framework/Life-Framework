// red-proof: observed 2026-11-17 with `tools\cli test --no-build --only LF_Test_GracePerCharacter` on the unfixed `LF_Grace.Protects` (it read `LF_Playtime.Total(accountUid)`): failed `- a fresh second character on a 10 hour account is protected` and `- one second before its own 5 minutes it is protected`. Fixed (it reads `LF_GraceCharacterClock.Seconds`); green.
// tier: LOGIC
//! #2443: the grace clock is the character's own played time, not its account's. A fresh second
//! character on an account with 10 hours played is protected; the same character after N minutes of
//! its own play (credited through LF_Playtime.Settle) is not; a character with a crime never is. A save
//! from before the grace clock seeds each existing character with its account's playtime.
class LF_Test_GracePerCharacter : LF_Test
{
	protected static const string UID = "gpc-account-uid";
	protected static const string UID_CROOK = "gpc-crook-uid";
	protected static const ResourceName PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";
	protected static const int PLAYER_ID = 924430;
	protected static const int TEN_HOURS = 36000;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "onboarding/grace-per-character";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_PlaytimeRecord> playtimeBefore = LF_Playtime.ExportAll();
		array<ref LF_CrimeRecordRow> crimesBefore = LF_CrimeRecord.ExportAll();
		array<ref LF_GraceClockRow> clocksBefore = LF_GraceCharacterClock.ExportAll();
		LF_Playtime.ApplyAll(null);
		LF_CrimeRecord.ApplyAll(null);
		LF_GraceCharacterClock.ApplyAll(null);
		LF_CharacterConfig.SetMaxPerAccountOverride(2);
		LF_Grace.SetMinutesOverride(5);

		// An old account: 10 hours played on its first character.
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account = LF_PlayerAccount.Create(UID);
		accounts.AddToCache(account);
		LF_PlayerCharacter first = LF_PlayerCharacter.Create(PREFAB, "Vera", "Veteran", 40);
		account.AddCharacter(first, true);
		LF_Playtime.Credit(UID, LF_Playtime.JOB_CIVILIAN, TEN_HOURS);
		LF_GraceCharacterClock.Credit(first.GetRecordKey(), TEN_HOURS);
		ctx.False(LF_Grace.Protects(first.GetRecordKey()), "the first character, 10 hours in, is past its grace");

		// A second, fresh character on that account.
		LF_PlayerCharacter second = LF_PlayerCharacter.Create(PREFAB, "Nina", "Newcomer", 20);
		account.AddCharacter(second, true);
		string secondKey = second.GetRecordKey();
		ctx.Equal(TEN_HOURS, LF_Playtime.Total(UID), "the account has 10 hours played");
		ctx.True(LF_Grace.Protects(secondKey), "a fresh second character on a 10 hour account is protected");

		// Its own play, through the playtime clock: 4:59 in it is still protected, at 5:00 it is not.
		int now = 1000000;
		ctx.True(LF_Playtime.StartClock(PLAYER_ID, UID, now), "the playtime clock starts for the account");
		LF_Playtime.Settle(PLAYER_ID, now + 299);
		ctx.Equal(299, LF_GraceCharacterClock.Seconds(secondKey), "the settle credits the active character");
		ctx.Equal(TEN_HOURS, LF_GraceCharacterClock.Seconds(first.GetRecordKey()), "the other character's clock does not move");
		ctx.True(LF_Grace.Protects(secondKey), "one second before its own 5 minutes it is protected");
		LF_Playtime.StopClock(PLAYER_ID, now + 300);
		ctx.Equal(300, LF_GraceCharacterClock.Seconds(secondKey), "the disconnect credits the last second");
		ctx.False(LF_Grace.Protects(secondKey), "after 5 minutes of its own play it is not protected");

		// A character with a crime is never protected, however fresh.
		ctx.True(LF_Grace.Protects(UID_CROOK), "a fresh character with no crime is protected");
		LF_CrimeRecord.Record(LF_OnCrimeCommitted.Create(UID_CROOK, LF_WantedReports.OFFENCE_SHOPLIFTING));
		ctx.False(LF_Grace.Protects(UID_CROOK), "a character with a crime is never protected");

		// The save seam and the old-save migration.
		array<ref LF_GraceClockRow> saved = LF_GraceCharacterClock.ExportAll();
		LF_GraceCharacterClock.ApplyAll(saved);
		LF_GraceCharacterClock.ApplyAll(saved);
		ctx.Equal(300, LF_GraceCharacterClock.Seconds(secondKey), "a second apply of the same rows gives the same clock");
		LF_GraceCharacterClock.SeedFromAccounts();
		ctx.Equal(LF_GraceCharacterClock.CAP_SECONDS, LF_GraceCharacterClock.Seconds(secondKey), "an old save seeds each character with its account playtime, capped");
		ctx.False(LF_Grace.Protects(secondKey), "so the upgrade gives no existing character a fresh grace");
		LF_Grace.SetMinutesOverride(720);
		ctx.False(LF_Grace.Protects(secondKey), "a grace longer than the clock cap still ends at the cap");

		accounts.RemoveFromCache(UID);
		LF_CharacterConfig.SetMaxPerAccountOverride(0);
		LF_Grace.SetMinutesOverride(-1);
		LF_Playtime.ApplyAll(playtimeBefore);
		LF_CrimeRecord.ApplyAll(crimesBefore);
		LF_GraceCharacterClock.ApplyAll(clocksBefore);
	}
}
