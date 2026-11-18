// red-proof: observed 2026-11-17 with `tools\cli test --no-build --only LF_Test_StunContact` on unfixed main (contact part only): FAIL `an on-duty officer's stun is police contact` and `the stunned player is inside the contact window`; the civilian cases passed. Green after the change. The reset hooks did not exist on main (the test did not compile against it).
//! A taser hit is police contact (#2543, review item 23): an on-duty officer who stuns a player stamps
//! "police contact" on the victim, so a wanted player who is tased and logs out falls under the
//! logout rule (LF_DisconnectRules). A stun from someone who is not an on-duty officer stamps nothing.
//! The stun and stun-buzz statics clear at world end (review item 27): both reset hooks empty the
//! counters and the playing map.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_StunContact : LF_Test
{
	protected static const string OFFICER_UID = "stun-contact-officer";
	protected static const string CIVILIAN_UID = "stun-contact-civilian";
	protected static const string VICTIM_UID = "stun-contact-victim";

	override string GetName()
	{
		return "police/stun-contact";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_MedicConfig config = LF_MedicConfig.Get();
		bool wasEnabled = config.m_bDownedEnabled;
		config.m_bDownedEnabled = true;
		LF_Downed.SetTestAllEligible(true);
		LF_Stun.ResetStatics();
		LF_PoliceContact.Forget(VICTIM_UID);

		RunContact(ctx);

		LF_PoliceContact.Forget(VICTIM_UID);
		config.m_bDownedEnabled = wasEnabled;
		LF_Downed.SetTestAllEligible(false);
		LF_Stun.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunContact(LF_TestContext ctx)
	{
		IEntity entity = LF_TestCharacterFixture.Spawn(ctx);
		if (!entity)
			return;

		SCR_ChimeraCharacter victim = SCR_ChimeraCharacter.Cast(entity);
		if (!victim)
		{
			ctx.Fail("the test body is not a SCR_ChimeraCharacter");
			LF_TestCharacterFixture.Delete(entity);
			return;
		}

		LF_PoliceRanks.BindOfficer(victim, VICTIM_UID);
		LF_PlayerAccount officer = LF_TestPoliceKit.Account(OFFICER_UID, true);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		LF_TestPoliceKit.Account(CIVILIAN_UID, false);

		// A stun from someone who is not an on-duty officer is no police contact.
		ctx.Equal(-1, LF_PoliceContact.AgeSecondsNow(VICTIM_UID), "no contact before the stun");
		ctx.True(LF_Stun.Apply(victim, 5, CIVILIAN_UID), "a civilian's stun is applied");
		ctx.Equal(-1, LF_PoliceContact.AgeSecondsNow(VICTIM_UID), "a stun from a civilian is not police contact");
		LF_Stun.End(victim);
		LF_Knockout.Settle(victim);

		// An on-duty officer's stun is.
		ctx.True(LF_Stun.Apply(victim, 5, OFFICER_UID), "an officer's stun is applied");
		int age = LF_PoliceContact.AgeSecondsNow(VICTIM_UID);
		ctx.True(age >= 0, "an on-duty officer's stun is police contact");
		ctx.True(LF_PoliceContact.InWindow(age, 120), "the stunned player is inside the contact window");
		LF_Stun.End(victim);
		LF_Knockout.Settle(victim);

		RunReset(ctx, entity.GetID());

		LF_TestPoliceKit.Release(OFFICER_UID);
		LF_TestPoliceKit.Release(CIVILIAN_UID);
		LF_TestCharacterFixture.Delete(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Both world-end hooks empty the stun counters and the buzz map.
	protected void RunReset(LF_TestContext ctx, EntityID bodyId)
	{
		LF_StunBuzz.MarkPlayingForTest(bodyId);
		ctx.True(LF_Stun.GetStunCount() > 0, "stuns are counted before the reset");
		ctx.Equal(1, LF_StunBuzz.GetPlayingCount(), "a buzz is playing before the reset");
		ctx.Equal(1, LF_StunBuzz.GetStartedCount(), "the buzz start is counted before the reset");

		LF_StaticResetHook stunHook = new LF_Stun_StaticResetHook();
		LF_StaticResetHook buzzHook = new LF_StunBuzz_StaticResetHook();
		stunHook.Apply();
		buzzHook.Apply();

		ctx.Equal(0, LF_Stun.GetStunCount(), "the stun hook clears the stun count");
		ctx.Equal(0, LF_Stun.GetAbsorbedCount(), "the stun hook clears the absorbed count");
		ctx.Equal(0, LF_StunBuzz.GetPlayingCount(), "the buzz hook empties the playing map");
		ctx.Equal(0, LF_StunBuzz.GetStartedCount(), "the buzz hook clears the started count");
		ctx.Equal(0, LF_StunBuzz.GetStoppedCount(), "the buzz hook clears the stopped count");
	}
}
