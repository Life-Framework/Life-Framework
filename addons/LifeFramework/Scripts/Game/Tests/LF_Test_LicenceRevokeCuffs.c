// red-proof: RED-OK licence/revoke-cuffs, observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_LicenceRevokeCuffs`:
// with the target not cuffed, `a cuffed target gets the revoke interaction`, `the server revokes from a cuffed target` and
// `the cuffed target's permit is gone` fail. Against the unfixed LF_LicenceDesk.c (`--only`): `a free target gets no revoke interaction`
// and the three cuffed-path lines fail; restored, green.
// tier: WORLD
//! licence/revoke-cuffs (#1916): an on-duty officer can revoke the firearm permit only from a
//! restrained target. A free target gets no "Revoke firearm permit" interaction and the server's
//! LF_LicenceDesk.Revoke refuses (the permit stays); once the target is cuffed the offer shows and
//! the revoke takes the permit.
class LF_Test_LicenceRevokeCuffs : LF_Test
{
	protected static const string OFFICER_UID = "lf-test-revoke-officer";
	protected static const string TARGET_UID = "lf-test-revoke-target";

	override string GetName()
	{
		return "licence/revoke-cuffs";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity officer = LF_TestCharacterFixture.Spawn(ctx);
		IEntity target = LF_TestCharacterFixture.Spawn(ctx);
		if (!officer || !target)
		{
			LF_TestCharacterFixture.Delete(officer);
			LF_TestCharacterFixture.Delete(target);
			return;
		}

		LF_Progression.Forget(TARGET_UID);
		LF_PlayerAccount officerAccount = LF_TestPoliceKit.Account(OFFICER_UID, true);
		officerAccount.SetFaction(LF_Faction.POLICE);
		officerAccount.SetOnDuty(true);
		LF_PoliceRanks.BindOfficer(officer, OFFICER_UID);
		LF_TestPoliceKit.Account(TARGET_UID, false);
		LF_PoliceRanks.BindOfficer(target, TARGET_UID);

		SCR_ChimeraCharacter chimera = SCR_ChimeraCharacter.Cast(target);
		LF_LicenseManagerComponent licences = LF_Component<LF_LicenseManagerComponent>.Find(target);
		ctx.NotNull(chimera, "the target is a character");
		ctx.NotNull(licences, "the target has a licence manager");
		if (!chimera || !licences)
		{
			Cleanup(officer, target);
			return;
		}

		licences.UnlockLicense(LF_LicenceDesk.PERMIT, true);
		ctx.True(licences.HasLicense(LF_LicenceDesk.PERMIT), "the target holds the firearm permit");

		LF_LicenceRevokeRole role = new LF_LicenceRevokeRole();
		ctx.False(LF_Restraint.IsRestrained(target), "the target starts free");
		ctx.False(role.CanOffer(officer, target), "a free target gets no revoke interaction");
		ctx.False(LF_LicenceDesk.Revoke(officer, target), "the server refuses to revoke from a free target");
		ctx.True(licences.HasLicense(LF_LicenceDesk.PERMIT), "so the free target keeps the permit");

		if (!RedProof())
			chimera.LF_SetRestrained(true);

		ctx.True(role.CanOffer(officer, target), "a cuffed target gets the revoke interaction");
		ctx.True(LF_LicenceDesk.Revoke(officer, target), "the server revokes from a cuffed target");
		ctx.False(licences.HasLicense(LF_LicenceDesk.PERMIT), "the cuffed target's permit is gone");
		ctx.False(role.CanOffer(officer, target), "and the interaction is gone once there is no permit");

		Cleanup(officer, target);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity officer, IEntity target)
	{
		LF_TestCharacterFixture.Delete(officer);
		LF_TestCharacterFixture.Delete(target);
		LF_Progression.Forget(TARGET_UID);
		LF_TestPoliceKit.Release(OFFICER_UID);
		LF_TestPoliceKit.Release(TARGET_UID);
	}
}
