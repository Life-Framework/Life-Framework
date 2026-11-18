// red-proof: 2026-11-17, a bug fix (crime review item 21, #2542): one boot against main's LF_JailEscort.c (only stub seams so it compiles) failed `- a grab from beyond reach is refused`, `- a second officer cannot take a held suspect`, `- the first hold stays (expected 'grab-rules-officer-a', got '')`; green with LF_JailEscort.Grab, GrabRefusal and the hold check.
//! An escort grab needs reach and never takes another officer's prisoner (#2542). The server side of
//! both escort actions runs LF_JailEscort.Grab: reach first (LF_PlayerInteraction.CheckPair, the rule
//! the cuff and arrest paths use), then the hold check inside LF_JailEscort.Escort.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_EscortGrabRules : LF_Test
{
	override string GetName()
	{
		return "jail/escort-grab-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		string officerA = "grab-rules-officer-a";
		string officerB = "grab-rules-officer-b";
		string prisonerUid = "grab-rules-prisoner";

		LF_JailEscort.ResetStatics();

		LF_PlayerAccount a = LF_PlayerAccountManager.GetOrCreate(officerA);
		a.SetFaction(LF_Faction.POLICE);
		a.SetOnDuty(true);
		a.SetPoliceRank(LF_PoliceRanks.SENIOR);
		LF_PlayerAccount b = LF_PlayerAccountManager.GetOrCreate(officerB);
		b.SetFaction(LF_Faction.POLICE);
		b.SetOnDuty(true);
		b.SetPoliceRank(LF_PoliceRanks.SENIOR);
		LF_PlayerAccount prisoner = LF_PlayerAccountManager.GetOrCreate(prisonerUid);
		prisoner.SetWantedLevel(2);

		// Reach: the pure rule every pair action shares, judged before anything is held.
		LF_EInteractionRefusal reachFar = LF_PlayerInteraction.CheckReach(true, false, true, true, true, LF_PlayerInteraction.REACH + 3.0);
		LF_EInteractionRefusal reachNear = LF_PlayerInteraction.CheckReach(true, false, true, true, true, 1.0);
		ctx.False(LF_JailEscort.GrabRefusal(reachFar, officerA, prisonerUid) == string.Empty, "a grab from beyond reach is refused");
		ctx.EqualStr("#LF-Interact_TooFar", LF_JailEscort.GrabRefusal(reachFar, officerA, prisonerUid), "the officer is told to step closer");
		ctx.False(LF_JailEscort.IsEscorted(prisonerUid), "a refused grab holds nobody");

		ctx.EqualStr(string.Empty, LF_JailEscort.GrabRefusal(reachNear, officerA, prisonerUid), "a grab within reach of a free suspect is allowed");
		ctx.True(LF_JailEscort.Escort(officerA, prisonerUid), "the first officer takes the suspect");
		ctx.True(LF_JailEscort.IsEscorted(prisonerUid), "the suspect is held");

		// Another officer's prisoner.
		ctx.EqualStr("#LF-Escort_HeldByOther", LF_JailEscort.GrabRefusal(reachNear, officerB, prisonerUid), "the second officer is told the suspect is held");
		ctx.EqualStr(string.Empty, LF_JailEscort.GrabRefusal(reachNear, officerA, prisonerUid), "the holder grabbing again is not refused");
		ctx.False(LF_JailEscort.Escort(officerB, prisonerUid), "a second officer cannot take a held suspect");
		ctx.EqualStr(officerA, LF_JailEscort.GetHolder(prisonerUid), "the first hold stays");
		ctx.EqualStr(prisonerUid, LF_JailEscort.GetOnFootPrisoner(officerA), "the first officer still has the suspect");
		ctx.EqualStr(string.Empty, LF_JailEscort.GetOnFootPrisoner(officerB), "the second officer has nobody");

		// The holder's own repeat grab keeps working.
		ctx.True(LF_JailEscort.Escort(officerA, prisonerUid), "the holder can grab again");

		// A released hold is free to take again.
		LF_JailEscort.ReleasePlayer(officerA);
		ctx.True(LF_JailEscort.Escort(officerB, prisonerUid), "once released, another officer can take the suspect");

		LF_JailEscort.ResetStatics();
	}
}
