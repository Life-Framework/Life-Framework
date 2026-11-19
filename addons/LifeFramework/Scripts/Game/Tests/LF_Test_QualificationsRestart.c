// red-proof: observed 2026-11-19 `tools/cli test --no-build --red LF_Test_QualificationsRestart`: RED PROOF OK, first `the first course is held after the load`. Green with `--only`. Restart: `tools/cli wt e2e slice-3425`.
//! Qualifications ride the real save path (#3425, the "qualifications" save section): two granted
//! courses survive a save and load and answer Has() by unlock id, loading twice gives the same set, a
//! saved course id the table does not list is dropped and logged, and a boot whose save predates the
//! section (OnAbsent()) leaves nobody holding a course. Across a restart (`tools\cli wt e2e`): every
//! non-reload run leaves the e2e character holding two courses; the reload boot finds them.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_QualificationsRestart : LF_Test
{
	static const string CHARACTER = "test-qualifications";
	//! The e2e character: every non-reload run seeds it, the reload boot reads it after the restart.
	static const string E2E_KEY = "e2e-qualifications";
	static const string COURSE_A = "detective_course";
	static const string COURSE_B = "first_aid_course";

	override string GetName()
	{
		return "qualifications/restart";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
#ifdef LF_E2E_LOAD_BOOT
		// The reload boot: the e2e character's courses came back from the real save, when it carried
		// the section (an older ref build wrote none; LF_E2E logs the skip).
		if (LF_E2E.RequiresSection(LF_QualificationsSavePayload.NAME))
			VerifyE2E(ctx);

		return;
#endif

		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));

		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		ctx.NotNull(LF_QualificationConfig.Get(), "the course table resolves");
		LF_SavePayload section = LF_SaveSections.Get(LF_QualificationsSavePayload.NAME);
		ctx.NotNull(section, "qualifications is a registered save section");
		if (!persistence || !section || !LF_QualificationConfig.Get())
			return;

		// The live set before the test, put back at the end.
		array<ref LF_QualificationRow> before = LF_Qualifications.ExportAll();

		ctx.True(LF_Qualifications.Grant(CHARACTER, COURSE_A), "the first course is granted");
		ctx.True(LF_Qualifications.Grant(CHARACTER, COURSE_B), "the second course is granted");
		ctx.False(LF_Qualifications.Grant(CHARACTER, COURSE_A), "a held course is not granted twice");
		ctx.False(LF_Qualifications.Grant(CHARACTER, "no_such_course"), "an unknown course is refused");
		ctx.True(LF_Qualifications.Has(CHARACTER, "detective"), "the first course unlocks detective");
		ctx.False(LF_Qualifications.Has(CHARACTER, "traffic"), "an unlock of a course not held is not held");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_QualificationsSavePayload.NAME), "the save carries the qualifications section");
		ctx.True(saved.Contains(CHARACTER), "the save carries the character's key");

		// Restart: the session holds no course.
		LF_Qualifications.ClearAll();
		ctx.False(LF_Qualifications.HasCourse(CHARACTER, COURSE_A), "the cleared session holds nothing");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		// RedProof(): the load is skipped, so every read-back assertion fails (`--red`).
		if (!RedProof())
			ReadSection(loadContext, section, ctx);

		ctx.True(LF_Qualifications.HasCourse(CHARACTER, COURSE_A), "the first course is held after the load");
		ctx.True(LF_Qualifications.HasCourse(CHARACTER, COURSE_B), "the second course is held after the load");
		ctx.True(LF_Qualifications.Has(CHARACTER, "first_aid"), "the second course's unlock is held after the load");

		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		ReadSection(again, section, ctx);
		array<string> held = {};
		LF_Qualifications.GetHeld(CHARACTER, held);
		ctx.Equal(2, held.Count(), "loading twice keeps the same two courses");

		// A saved course id the table does not list is dropped; the rest of the rows apply.
		array<ref LF_QualificationRow> rows = LF_Qualifications.ExportAll();
		int known = rows.Count();
		rows.Insert(LF_QualificationRow.Create(CHARACTER, "retired_course"));
		ctx.Equal(known, LF_Qualifications.ApplyAll(rows), "an unknown course id is dropped on load");
		ctx.False(LF_Qualifications.HasCourse(CHARACTER, "retired_course"), "the unknown course is not held");
		ctx.True(LF_Qualifications.HasCourse(CHARACTER, COURSE_A), "the known rows still apply");

		// A boot (an empty session) loading a save from before the section: nobody holds a course.
		LF_Qualifications.ClearAll();
		section.OnAbsent();
		ctx.False(LF_Qualifications.HasCourse(CHARACTER, COURSE_A), "OnAbsent leaves no course held");
		ctx.False(LF_Qualifications.Has(CHARACTER, "detective"), "OnAbsent leaves no unlock held");

		LF_Qualifications.ApplyAll(before);
		// Every non-reload run (the e2e save boot among them) leaves the e2e character holding two courses.
		SeedE2E(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the qualifications section of a whole-store JSON save, as LF_SaveSections.ReadAll does.
	protected static void ReadSection(notnull JsonLoadContext context, notnull LF_SavePayload section, LF_TestContext ctx)
	{
		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_QualificationsSavePayload.NAME), "the qualifications section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the qualifications section reads");
		context.EndObject();
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e save boot: one character holds two courses.
	static void SeedE2E(LF_TestContext ctx)
	{
		LF_Qualifications.Grant(E2E_KEY, COURSE_A);
		LF_Qualifications.Grant(E2E_KEY, COURSE_B);
		ctx.True(LF_Qualifications.HasCourse(E2E_KEY, COURSE_A) && LF_Qualifications.HasCourse(E2E_KEY, COURSE_B), "the e2e character holds two courses before the save");
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot: the same two courses after the restart.
	static void VerifyE2E(LF_TestContext ctx)
	{
		ctx.True(LF_Qualifications.HasCourse(E2E_KEY, COURSE_A), "the e2e character's first course survives the restart");
		ctx.True(LF_Qualifications.HasCourse(E2E_KEY, COURSE_B), "the e2e character's second course survives the restart");
		ctx.True(LF_Qualifications.Has(E2E_KEY, "detective"), "the e2e character's unlock survives the restart");
		LF_Debug.Info(LF_QualificationConfig.FEATURE, string.Format("e2e restart: %1 holds %2 and %3: %4", E2E_KEY, COURSE_A, COURSE_B, LF_Qualifications.HasCourse(E2E_KEY, COURSE_A) && LF_Qualifications.HasCourse(E2E_KEY, COURSE_B)));
	}
}
