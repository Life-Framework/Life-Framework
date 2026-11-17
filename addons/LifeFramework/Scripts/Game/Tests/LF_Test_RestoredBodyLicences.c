// red-proof: RED-OK licence/restored-body, observed 2026-11-17 with `tools\cli test --no-build --red LF_Test_RestoredBodyLicences,LF_Test_IdRegistryUnreadable`:
// with the adopt call skipped, `the restored body holds the saved firearm permit` and `the licence desk sees the permit and does not charge again` fail. Green with the fix (--only).
// tier: WORLD
//! licence/restored-body (#2413): a player who takes control of a living body a save restored is
//! never spawned a new one (LF_SpawnLogic "already controls a character"), so the progression apply
//! every spawned body gets never ran: the body held only its starter licences and the licence desk
//! charged a permit holder again. LF_SpawnLogic.AdoptLivingBody gives that body the saved licences.
class LF_Test_RestoredBodyLicences : LF_Test
{
	protected static const string UID = "lf-test-restored-body";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "licence/restored-body";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (!body)
			return;

		LF_LicenseManagerComponent licences = LF_Component<LF_LicenseManagerComponent>.Find(body);
		ctx.NotNull(licences, "the body has a licence manager");
		if (!licences)
		{
			LF_TestCharacterFixture.Delete(body);
			return;
		}

		LF_Progression.Forget(UID);
		array<LF_ELicenseType> saved = {LF_ELicenseType.UNEMPLOYED, LF_ELicenseType.FIREARM_PERMIT};
		LF_Progression.NoteLicenses(UID, saved);
		ctx.False(licences.HasLicense(LF_LicenceDesk.PERMIT), "the restored body starts with only its starter licences");
		ctx.False(LF_LicenceDesk.Holds(body, UID), "so the licence desk sees no permit on it");

		if (!RedProof())
			ctx.True(LF_SpawnLogic.AdoptLivingBody(UID, body), "the saved record applies to the restored body");

		ctx.True(licences.HasLicense(LF_LicenceDesk.PERMIT), "the restored body holds the saved firearm permit");
		ctx.True(LF_LicenceDesk.Holds(body, UID), "the licence desk sees the permit and does not charge again");

		// Idempotent: a second hand-over leaves the same licences.
		if (!RedProof())
			LF_SpawnLogic.AdoptLivingBody(UID, body);
		ctx.Equal(2, licences.GetUnlockedLicenses().Count(), "applying twice adds no licence");

		// A broken input degrades this one feature: no body, or no character key, applies nothing.
		ctx.False(LF_SpawnLogic.AdoptLivingBody(UID, null), "no body: nothing applied");
		ctx.False(LF_SpawnLogic.AdoptLivingBody("", body), "no character key: nothing applied");

		LF_Progression.Forget(UID);
		LF_TestCharacterFixture.Delete(body);
	}
}
