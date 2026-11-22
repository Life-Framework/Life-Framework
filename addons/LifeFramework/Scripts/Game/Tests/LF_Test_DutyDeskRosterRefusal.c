// red-proof: 2026-11-22 `tools\cli test --no-build --red LF_Test_DutyDeskRosterRefusal`: `RED-OK police/duty-desk-roster-refusal (fails with the hook on, 6 assertions)`; RedProof expects the old client greying.

//! rc 0.2.0 (overnight island smoke): a player off the police roster pressing the duty desk got `cannot_perform` and
//! the server logged nothing: the client greyed the press, so the server never refused it. The client now greys only
//! a missing account and the wrong desk; a press off the roster reaches the server, whose RefuseClockIn returns
//! #LF-Duty_NotOnRoster, logged by ClockIn and toasted by the desk.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_DutyDeskRosterRefusal : LF_Test
{
	protected static const string OUTSIDER = "duty-desk-roster-refusal";

	override string GetName()
	{
		return "police/duty-desk-roster-refusal";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		int offRoster = LF_AccountStateBits.Encode(true, LF_Faction.CIVILIAN, false, 0, 0, false);
		string expect = "";
		if (RedProof())
			expect = "#LF-Duty_NotOnRoster";

		ctx.EqualStr(expect, LF_DutyAction.ClientRefusal(offRoster, LF_Faction.POLICE), "a press off the roster reaches the server");
		ctx.EqualStr("", LF_DutyAction.ClientRefusal(offRoster, LF_Faction.MEDIC), "the medic desk is open to a registered player");
		ctx.EqualStr("#LF-Duty_NoAccount", LF_DutyAction.ClientRefusal(0, LF_Faction.POLICE), "no account is still greyed on the client");
		int medicOnDuty = LF_AccountStateBits.Encode(true, LF_Faction.MEDIC, true, 0, 0, false);
		ctx.EqualStr("#LF-Duty_WrongDesk", LF_DutyAction.ClientRefusal(medicOnDuty, LF_Faction.POLICE), "on duty at the other service is still greyed");

		// The server refuses the press with the roster key, which ClockIn logs and the desk toasts.
		LF_PlayerAccount outsider = LF_TestPoliceKit.Account(OUTSIDER, false);
		ctx.EqualStr("#LF-Duty_NotOnRoster", LF_DutyLocker.ClockIn(outsider, null, LF_Faction.POLICE), "the server refuses a clock-in off the roster");
	}
}
