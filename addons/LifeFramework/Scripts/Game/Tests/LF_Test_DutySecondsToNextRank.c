// red-proof: with `RedProof()` on no ladder is installed and no account is asked, so every answer reads -1: the
// "seconds left", "already due", "top rank", "civilian" and medic checks all fail.

//! police/duty-seconds-to-next-rank: LF_PoliceRanks.SecondsToNextRank, the on-duty seconds an account still has to serve
//! before its next time promotion (the duty clock the promotion sweep and the roster read). Police count the police
//! on-duty clock and medics the medic one; the answer is the rung's total minus what is served; the top rung, a rung that
//! is already due (a promotion that could not happen) and a civilian have no time promotion ahead (int.MAX).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_DutySecondsToNextRank : LF_Test
{
	protected static const string UID_FRESH = "duty-next-fresh";
	protected static const string UID_PART = "duty-next-part";
	protected static const string UID_DUE = "duty-next-due";
	protected static const string UID_SECOND = "duty-next-second";
	protected static const string UID_TOP = "duty-next-top";
	protected static const string UID_CIV = "duty-next-civilian";
	protected static const string UID_MEDIC = "duty-next-medic";
	protected static const string UID_MEDIC_TOP = "duty-next-medic-top";
	protected static const string UID_BOTH = "duty-next-medic-with-police-time";

	override string GetName()
	{
		return "police/duty-seconds-to-next-rank";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PoliceConfig Ladder()
	{
		LF_PoliceConfig config = new LF_PoliceConfig();
		config.m_aRanks = {};
		config.m_aRanks.Insert(LF_PoliceRank.Create("rung0", "EL-Rank_Patrol", 1000, -1, 3600, ResourceName.Empty));
		config.m_aRanks.Insert(LF_PoliceRank.Create("rung1", "EL-Rank_Patrol", 1000, -1, 10800, ResourceName.Empty));
		config.m_aRanks.Insert(LF_PoliceRank.Create("rung2", "EL-Rank_Patrol", 1000, -1, 0, ResourceName.Empty));
		config.m_iShippedRanks = 3;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PlayerAccount Make(string uid, LF_Faction faction, int policeRank, int policeSeconds)
	{
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(uid);
		account.SetFaction(faction);
		account.SetPoliceRank(policeRank);
		account.SetOnDutySeconds(policeSeconds);
		return account;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		int fresh = -1;
		int part = -1;
		int due = -1;
		int second = -1;
		int top = -1;
		int civilian = -1;
		int medic = -1;
		int medicExpected = -2;
		int medicTop = -1;
		int medicIgnoresPolice = -1;
		int medicIgnoresPoliceExpected = -2;
		if (!RedProof())
		{
			LF_PoliceConfig.UseForTest(Ladder());
			fresh = LF_PoliceRanks.SecondsToNextRank(Make(UID_FRESH, LF_Faction.POLICE, 0, 0));
			part = LF_PoliceRanks.SecondsToNextRank(Make(UID_PART, LF_Faction.POLICE, 0, 1000));
			due = LF_PoliceRanks.SecondsToNextRank(Make(UID_DUE, LF_Faction.POLICE, 0, 3600));
			second = LF_PoliceRanks.SecondsToNextRank(Make(UID_SECOND, LF_Faction.POLICE, 1, 10000));
			top = LF_PoliceRanks.SecondsToNextRank(Make(UID_TOP, LF_Faction.POLICE, 2, 999999));
			civilian = LF_PoliceRanks.SecondsToNextRank(Make(UID_CIV, LF_Faction.CIVILIAN, 0, 100));
			LF_PoliceConfig.Reload();

			// Medics: the shipped ladder, 10 s served on the medic clock. Police time on the same account does not count.
			LF_PlayerAccount medicAccount = LF_PlayerAccountManager.GetOrCreate(UID_MEDIC);
			medicAccount.SetFaction(LF_Faction.MEDIC);
			medicAccount.SetMedicRank(0);
			medicAccount.SetMedicDutySeconds(10);
			medic = LF_PoliceRanks.SecondsToNextRank(medicAccount);
			int required = LF_MedicRanks.DutySecondsForNext(0);
			medicExpected = int.MAX;
			if (required > 10)
				medicExpected = required - 10;

			LF_PlayerAccount topMedic = LF_PlayerAccountManager.GetOrCreate(UID_MEDIC_TOP);
			topMedic.SetFaction(LF_Faction.MEDIC);
			topMedic.SetMedicRank(LF_MedicRanks.TopRank());
			topMedic.SetMedicDutySeconds(99999999);
			medicTop = LF_PoliceRanks.SecondsToNextRank(topMedic);

			LF_PlayerAccount both = LF_PlayerAccountManager.GetOrCreate(UID_BOTH);
			both.SetFaction(LF_Faction.MEDIC);
			both.SetMedicRank(0);
			both.SetMedicDutySeconds(10);
			both.SetOnDutySeconds(5000000);
			medicIgnoresPolice = LF_PoliceRanks.SecondsToNextRank(both);
			medicIgnoresPoliceExpected = medicExpected;
		}

		ctx.Equal(3600, fresh, "a new recruit has the whole first rung to serve");
		ctx.Equal(2600, part, "1000 s served leaves the rest of the rung");
		ctx.Equal(int.MAX, due, "a rung already served has no clock left (the promotion sweep owns it)");
		ctx.Equal(800, second, "the second rung counts the total served, not time since the last promotion");
		ctx.Equal(int.MAX, top, "the top rung has no time promotion");
		ctx.Equal(int.MAX, civilian, "a civilian has no duty clock");
		ctx.Equal(medicExpected, medic, "a medic counts the medic clock against the medic ladder");
		ctx.Equal(int.MAX, medicTop, "the top medic rung has no time promotion");
		ctx.Equal(medicIgnoresPoliceExpected, medicIgnoresPolice, "police on-duty time does not move a medic's clock");
	}
}
