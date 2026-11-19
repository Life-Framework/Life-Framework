// red-proof: observed with `tools\cli test --no-build --only LF_Test_FishingLicenceGate` before the gate existed (scripts did not compile: LF_FishingLicenceGate and m_eRequiredLicence missing); then RED-OK from `--red` (RedProof() leaves the licence unnamed, so the missing-licence refusal assertions fail).
//! fishing/licence-gate (#3449): when the fishing config names a licence, a cast without it is refused
//! (NO_LICENCE, before any rod or bait check) and the refusal line says where to get it and the fee
//! (#3150); holding the licence lets the cast go on to the next check; no licence named means casting
//! is open as today.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_FishingLicenceGate : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "fishing/licence-gate";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The pure rule.
		ctx.True(LF_FishingLicenceGate.Evaluate(LF_ELicenseType.UNEMPLOYED, false), "no licence named: open to everyone");
		ctx.False(LF_FishingLicenceGate.Evaluate(LF_LicenceDesk.DRIVING, false), "a licence named and missing: refused");
		ctx.True(LF_FishingLicenceGate.Evaluate(LF_LicenceDesk.DRIVING, true), "a licence named and held: allowed");
		ctx.EqualStr("", LF_FishingLicenceGate.Where(LF_ELicenseType.UNEMPLOYED), "no licence named: no where line");
		ctx.EqualStr(LF_LicenceWhere.Line(LF_LicenceDesk.DRIVING), LF_FishingLicenceGate.Where(LF_LicenceDesk.DRIVING), "the refusal carries the #3150 where line and fee");

		LF_FishingConfig fresh = LF_FishingConfig.MakeEmpty();
		ctx.True(fresh.m_eRequiredLicence == LF_ELicenseType.UNEMPLOYED, "the config names no licence by default");

		IEntity angler = LF_TestCharacterFixture.Spawn(ctx);
		if (!angler)
			return;

		LF_LicenseManagerComponent licences = LF_Component<LF_LicenseManagerComponent>.Find(angler);
		ctx.NotNull(licences, "the test character has a licence manager");
		if (!licences)
		{
			LF_TestCharacterFixture.Delete(angler);
			return;
		}

		vector target = angler.GetOrigin();
		LF_FishingConfig config = LF_FishingConfig.MakeEmpty();
		LF_FishingConfig.UseForTest(config);

		// None named: the cast is not refused for a licence (it fails later, with no rod in the hands).
		ctx.True(LF_FishingLicenceGate.Allows(angler, config), "none named: the angler may cast");
		ctx.True(LF_FishingSession.CastOn(angler, target, LF_FishingConfig.WATER_OCEAN, 0) != LF_EFishingRefusal.NO_LICENCE, "none named: a cast is not refused for a licence");

		// Named and missing: refused with the licence refusal.
		if (!RedProof())
			config.m_eRequiredLicence = LF_LicenceDesk.DRIVING;

		ctx.False(LF_FishingLicenceGate.Allows(angler, config), "named and missing: the gate refuses");
		ctx.True(LF_FishingSession.CastOn(angler, target, LF_FishingConfig.WATER_OCEAN, 0) == LF_EFishingRefusal.NO_LICENCE, "named and missing: the cast is refused for the licence");

		// Named and held: the cast passes the gate.
		licences.UnlockLicense(LF_LicenceDesk.DRIVING, true);
		ctx.True(licences.HasLicense(LF_LicenceDesk.DRIVING), "the angler now holds the licence");
		ctx.True(LF_FishingLicenceGate.Allows(angler, config), "named and held: the gate allows");
		ctx.True(LF_FishingSession.CastOn(angler, target, LF_FishingConfig.WATER_OCEAN, 0) != LF_EFishingRefusal.NO_LICENCE, "named and held: a cast is not refused for a licence");

		LF_FishingConfig.Reload();
		LF_TestCharacterFixture.Delete(angler);
	}
}
