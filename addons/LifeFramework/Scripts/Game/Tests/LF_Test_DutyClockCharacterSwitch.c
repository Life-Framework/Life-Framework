// red-proof: #2838, 2026-11-18 `tools/cli test --no-build --only LF_Test_DutyClockCharacterSwitch` on the unfixed account: FAIL - the switch credits the 600 s to the outgoing character (expected 600, got 0), - the switch stops the clock, - a later clock-out credits nothing to the incoming character (expected 0, got 1600), and four more. Green with SettleDutyBeforeCharacterChange.
//! police/duty-clock-character-switch (#2838): a running duty clock is settled for the outgoing
//! character before the account's active character changes (pick, a new character made active, the
//! active one removed), so its on-duty seconds never land on the next character. Logout already
//! settles at the disconnect (#2622); covered here too.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_DutyClockCharacterSwitch : LF_Test
{
	protected static const string UID = "duty-switch-account";
	protected static const string PREFAB = "Prefabs/Characters/Core/Character_Roleplay.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/duty-clock-character-switch";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		int now = 0;
		if (GetGame() && GetGame().GetWorld())
			now = Math.Round(GetGame().GetWorld().GetWorldTime());

		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(UID);
		LF_PlayerCharacter a = LF_PlayerCharacter.Create(PREFAB, "Ada", "Switch", 30);
		LF_PlayerCharacter b = LF_PlayerCharacter.Create(PREFAB, "Bea", "Switch", 31);
		LF_PlayerCharacter c = LF_PlayerCharacter.Create(PREFAB, "Cy", "Switch", 32);
		account.AddCharacter(a, true);
		account.AddCharacter(b);
		account.SetFaction(LF_Faction.POLICE);
		account.SetOnDuty(true);
		account.ResetShiftSeconds();
		account.TakeUnpaidDutySeconds();

		// A has been on duty for 600 s when the account switches to B.
		account.StartDutyClock(now - 600000);
		account.SetActiveCharacter(b);
		ctx.Equal(600, a.GetStanding().m_iOnDutySeconds, "the switch credits the 600 s to the outgoing character");
		ctx.Equal(0, b.GetStanding().m_iOnDutySeconds, "the incoming character gets none of them");
		ctx.False(account.IsDutyClockRunning(), "the switch stops the clock");
		LF_PoliceRanks.SettleDutyAt(account, false, now + 1000000);
		ctx.Equal(0, b.GetStanding().m_iOnDutySeconds, "a later clock-out credits nothing to the incoming character");

		// A new character made active settles the running clock for the one before it.
		account.StartDutyClock(now - 120000);
		account.AddCharacter(c, true);
		ctx.Equal(120, b.GetStanding().m_iOnDutySeconds, "making a new character active credits the 120 s to the one before");
		ctx.Equal(0, c.GetStanding().m_iOnDutySeconds, "the new character starts at 0");

		// Removing the active character settles its clock before it goes.
		account.StartDutyClock(now - 60000);
		account.RemoveCharacter(c);
		ctx.Equal(600, a.GetStanding().m_iOnDutySeconds, "removing the active character credits none of its time to the fallback");

		// Logout: the disconnect settles on the character that was playing.
		account.SetActiveCharacter(b);
		account.StartDutyClock(now - 30000);
		LF_PoliceRanks.StopDutyClockAt(account, now);
		account.SetActiveCharacter(a);
		ctx.Equal(150, b.GetStanding().m_iOnDutySeconds, "logout credits the 30 s to the character that was playing");
		ctx.Equal(600, a.GetStanding().m_iOnDutySeconds, "the next character is untouched by logout");

		account.SetOnDuty(false);
		account.StopDutyClock();
		account.SetFaction(LF_Faction.CIVILIAN);
		account.TakeUnpaidDutySeconds();
		account.RemoveCharacter(a);
		account.RemoveCharacter(b);
	}
}
