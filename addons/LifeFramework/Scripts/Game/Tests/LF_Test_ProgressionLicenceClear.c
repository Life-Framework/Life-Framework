// red-proof: with `RedProof()` on neither the flag clear nor the licence clear runs, so the flag stays set and the
// licences stay held and the "forgets", "keeps the record" and "returns how many" checks fail.

//! account/progression-clear: what the progression store forgets on request. ClearFlag drops one account flag and
//! leaves the other turn-ins and flags, and ClearAllLicences (the licences-persistence switch, #689) empties every
//! record's licence list but keeps the record, counts only the records that held a licence, and a second call finds nothing.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ProgressionLicenceClear : LF_Test
{
	protected static const string ADA = "test-progression-clear-ada";
	protected static const string BO = "test-progression-clear-bo";
	protected static const string CY = "test-progression-clear-cy";

	override string GetName()
	{
		return "account/progression-clear";
	}

	override void Run(LF_TestContext ctx)
	{
		// Anything already in the store (a suite neighbour) is put back afterwards.
		map<string, ref array<LF_ELicenseType>> held = new map<string, ref array<LF_ELicenseType>>();
		array<ref LF_ProgressionRecord> existing = LF_Progression.ExportAll();
		foreach (LF_ProgressionRecord record : existing)
		{
			if (!record || record.m_sPlayerUid.IsEmpty())
				continue;

			array<LF_ELicenseType> kept = {};
			LF_Progression.GetLicenses(record.m_sPlayerUid, kept);
			held.Set(record.m_sPlayerUid, kept);
		}

		LF_Progression.Forget(ADA);
		LF_Progression.Forget(BO);
		LF_Progression.Forget(CY);

		LF_Progression.NoteTurnedIn(ADA, "test-quest");
		LF_Progression.NoteFlag(ADA, "quiz");
		LF_Progression.NoteFlag(ADA, "other");
		ctx.True(LF_Progression.HasFlag(ADA, "quiz"), "setup: the quiz flag is set");

		bool quizAfter = true;
		bool otherAfter = false;
		bool questAfter = false;
		if (!RedProof())
		{
			LF_Progression.ClearFlag(ADA, "quiz");
			LF_Progression.ClearFlag("test-progression-clear-nobody", "quiz");
		}

		quizAfter = LF_Progression.HasFlag(ADA, "quiz");
		otherAfter = LF_Progression.HasFlag(ADA, "other");
		questAfter = LF_Progression.HasTurnedIn(ADA, "test-quest");
		ctx.False(quizAfter, "ClearFlag drops the flag");
		ctx.True(otherAfter, "ClearFlag leaves another flag alone");
		ctx.True(questAfter, "ClearFlag leaves a real turn-in alone");
		ctx.False(LF_Progression.HasFlag("test-progression-clear-nobody", "quiz"), "clearing a flag for a player with no record makes no record");
		ctx.True(LF_Progression.Get("test-progression-clear-nobody") == null, "no record was created for the unknown player");

		array<LF_ELicenseType> adaLicences = {LF_ELicenseType.FIREARM_PERMIT, LF_ELicenseType.DRIVING_LICENCE};
		array<LF_ELicenseType> boLicences = {LF_ELicenseType.DRIVING_LICENCE};
		LF_Progression.NoteLicenses(ADA, adaLicences);
		LF_Progression.NoteLicenses(BO, boLicences);
		LF_Progression.NoteTurnedIn(CY, "test-quest");

		array<LF_ELicenseType> readBack = {};
		ctx.Equal(2, LF_Progression.GetLicenses(ADA, readBack), "setup: Ada holds two licences");

		int cleared = 0;
		int clearedAgain = -1;
		if (!RedProof())
		{
			cleared = LF_Progression.ClearAllLicences();
			clearedAgain = LF_Progression.ClearAllLicences();
		}

		ctx.True(cleared >= 2, "ClearAllLicences counts the records that held a licence (at least Ada and Bo)");
		ctx.Equal(0, clearedAgain, "a second clear finds no licence left");
		array<LF_ELicenseType> adaAfter = {};
		array<LF_ELicenseType> boAfter = {};
		ctx.Equal(0, LF_Progression.GetLicenses(ADA, adaAfter), "Ada holds no licence after the clear");
		ctx.Equal(0, LF_Progression.GetLicenses(BO, boAfter), "Bo holds no licence after the clear");
		ctx.True(LF_Progression.Get(ADA) != null, "the record itself is kept");
		ctx.True(LF_Progression.HasTurnedIn(ADA, "test-quest"), "the clear leaves quest turn-ins alone");
		ctx.True(LF_Progression.HasTurnedIn(CY, "test-quest"), "a record that held no licence is untouched");

		LF_Progression.Forget(ADA);
		LF_Progression.Forget(BO);
		LF_Progression.Forget(CY);
		foreach (string uid, array<LF_ELicenseType> licences : held)
		{
			if (!licences.IsEmpty())
				LF_Progression.NoteLicenses(uid, licences);
		}
	}
}
