//------------------------------------------------------------------------------------------------
//! The medic job is a duty role (#258): clocking in at the hospital desk (LF_DutyAction with the
//! MEDIC role) swaps to the medic inventory and puts the account on duty as LF_Faction.MEDIC,
//! the same way the police desk does for POLICE. Service routing reads it as the medic job, so
//! the hospital board, taking a call and the pager reach an on-duty medic.
class LF_MedicDuty
{
	//------------------------------------------------------------------------------------------------
	//! Server: is this account clocked in as a medic?
	static bool IsOnDuty(LF_PlayerAccount account)
	{
		return account && account.IsOnDuty() && account.GetFaction() == LF_Faction.MEDIC;
	}

	//------------------------------------------------------------------------------------------------
	//! Client or server: the same from the packed account state.
	static bool IsOnDutyState(int state)
	{
		return LF_AccountStateBits.HasAccount(state) && LF_AccountStateBits.IsOnDuty(state) && LF_AccountStateBits.GetFaction(state) == LF_Faction.MEDIC;
	}
}
