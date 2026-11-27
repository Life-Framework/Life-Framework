// red-proof: with `RedProof()` on the catalogue helpers are not asked, so every parse is refused, every key is empty and
// no licence is criminal, which fails the licence-name, criminal-range, job-key and org-right checks.

//! catalogue/names: the small rules behind the item catalogue's refusals (LF_ItemCatalogue). A licence is read from its
//! enum name exactly (a typo or a number is no licence, so a bad config row cannot grant one), the criminal range is the
//! six black-market licences and nothing else (a counter that serves the wanted keys off it), a refusal's job and
//! organisation right resolve to their localization key, an unknown job falling to the unknown-job key and an unknown right shown as written.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ItemCatalogueNames : LF_Test
{
	override string GetName()
	{
		return "catalogue/names";
	}

	override void Run(LF_TestContext ctx)
	{
		bool firearmOk = false;
		LF_ELicenseType firearm = LF_ELicenseType.UNEMPLOYED;
		bool weedOk = false;
		LF_ELicenseType weed = LF_ELicenseType.UNEMPLOYED;
		bool typoOk = true;
		bool lowerOk = true;
		bool numberOk = true;
		bool emptyOk = true;
		bool criminalFirst = false;
		bool criminalLast = false;
		bool beforeRange = true;
		bool afterRange = true;
		bool drivingCriminal = true;
		bool unemployedCriminal = true;
		string police = "-";
		string medic = "-";
		string lumberjack = "-";
		string unknownJob = "-";
		string emptyJob = "-";
		string lowerJob = "-";
		string withdraw = "-";
		string spendFund = "-";
		string unknownRight = "-";
		if (!RedProof())
		{
			LF_ELicenseType parsed;
			firearmOk = LF_ItemCatalogue.ParseLicence("FIREARM_PERMIT", firearm);
			weedOk = LF_ItemCatalogue.ParseLicence("CRIMINAL_WEED", weed);
			typoOk = LF_ItemCatalogue.ParseLicence("FIREARM_PERMITT", parsed);
			lowerOk = LF_ItemCatalogue.ParseLicence("firearm_permit", parsed);
			numberOk = LF_ItemCatalogue.ParseLicence("300", parsed);
			emptyOk = LF_ItemCatalogue.ParseLicence("", parsed);
			criminalFirst = LF_ItemCatalogue.IsCriminalLicence(LF_ELicenseType.CRIMINAL_WEED);
			criminalLast = LF_ItemCatalogue.IsCriminalLicence(LF_ELicenseType.CRIMINAL_CLOTHES);
			beforeRange = LF_ItemCatalogue.IsCriminalLicence(LF_ELicenseType.POLICE_SEARCH);
			afterRange = LF_ItemCatalogue.IsCriminalLicence(LF_ELicenseType.FIREARM_PERMIT);
			drivingCriminal = LF_ItemCatalogue.IsCriminalLicence(LF_ELicenseType.DRIVING_LICENCE);
			unemployedCriminal = LF_ItemCatalogue.IsCriminalLicence(LF_ELicenseType.UNEMPLOYED);
			police = LF_ItemCatalogue.JobNameKey("POLICE");
			medic = LF_ItemCatalogue.JobNameKey("MEDIC");
			lumberjack = LF_ItemCatalogue.JobNameKey("LUMBERJACK");
			unknownJob = LF_ItemCatalogue.JobNameKey("ASTRONAUT");
			emptyJob = LF_ItemCatalogue.JobNameKey("");
			lowerJob = LF_ItemCatalogue.JobNameKey("police");
			withdraw = LF_ItemCatalogue.OrgRightNameKey("WITHDRAW");
			spendFund = LF_ItemCatalogue.OrgRightNameKey("SPEND_FUND");
			unknownRight = LF_ItemCatalogue.OrgRightNameKey("FLY");
		}

		ctx.True(firearmOk, "a licence name parses");
		ctx.Equal(LF_ELicenseType.FIREARM_PERMIT, firearm, "the firearm permit is read");
		ctx.True(weedOk, "a criminal licence name parses");
		ctx.Equal(LF_ELicenseType.CRIMINAL_WEED, weed, "the weed licence is read");
		ctx.False(typoOk, "a misspelt licence name is no licence");
		ctx.False(lowerOk, "the name is exact: lower case is no licence");
		ctx.False(numberOk, "a number is no licence name");
		ctx.False(emptyOk, "an empty name is no licence");

		ctx.True(criminalFirst, "the first black-market licence is criminal");
		ctx.True(criminalLast, "the last black-market licence is criminal");
		ctx.False(beforeRange, "a police licence is not criminal");
		ctx.False(afterRange, "the firearm permit is lawful");
		ctx.False(drivingCriminal, "the driving licence is lawful");
		ctx.False(unemployedCriminal, "the free starting licence is lawful");

		ctx.EqualStr("#LF-Job_Name_Police", police, "the police job key");
		ctx.EqualStr("#LF-Job_Name_Medic", medic, "the medic job key");
		ctx.EqualStr("#LF-Job_Name_Lumberjack", lumberjack, "the lumberjack job key");
		ctx.EqualStr("#LF-Job_Name_Unknown", unknownJob, "an unnamed job reads as the unknown job");
		ctx.EqualStr("#LF-Job_Name_Unknown", emptyJob, "no job reads as the unknown job");
		ctx.EqualStr("#LF-Job_Name_Unknown", lowerJob, "a job id is exact: lower case is the unknown job");
		ctx.EqualStr("#LF-OrgRight_WITHDRAW", withdraw, "the withdraw right's key");
		ctx.EqualStr("#LF-OrgRight_SPEND_FUND", spendFund, "the spend-fund right's key");
		ctx.EqualStr("FLY", unknownRight, "an unknown right is shown as written, not as a made-up key");
	}
}
