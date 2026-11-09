// red-proof: change GetLicenseWhitelistJob to return POLICE for MEDIC_ACCESS
// (or drop a POLICE_* case) and run `tools\cli test --tier fast`; the mapping
// assertions fail.
// tier: LOGIC
class LF_Test_LicenseWhitelist : LF_Test
{
	override string GetName()
	{
		return "license/whitelist-job-mapping";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.Equal(LF_EJobType.POLICE, LF_LicenseManagerComponent.GetLicenseWhitelistJob(LF_ELicenseType.POLICE_ACCESS), "POLICE_ACCESS gates on the POLICE whitelist");
		ctx.Equal(LF_EJobType.POLICE, LF_LicenseManagerComponent.GetLicenseWhitelistJob(LF_ELicenseType.POLICE_ARREST), "POLICE_ARREST gates on the POLICE whitelist");
		ctx.Equal(LF_EJobType.POLICE, LF_LicenseManagerComponent.GetLicenseWhitelistJob(LF_ELicenseType.POLICE_FINE), "POLICE_FINE gates on the POLICE whitelist");
		ctx.Equal(LF_EJobType.POLICE, LF_LicenseManagerComponent.GetLicenseWhitelistJob(LF_ELicenseType.POLICE_SEARCH), "POLICE_SEARCH gates on the POLICE whitelist");

		ctx.Equal(LF_EJobType.MEDIC, LF_LicenseManagerComponent.GetLicenseWhitelistJob(LF_ELicenseType.MEDIC_ACCESS), "MEDIC_ACCESS gates on the MEDIC whitelist, not POLICE");

		ctx.Equal(LF_EJobType.UNEMPLOYED, LF_LicenseManagerComponent.GetLicenseWhitelistJob(LF_ELicenseType.FARMER_APPLE), "FARMER_APPLE has no whitelist");
		ctx.Equal(LF_EJobType.UNEMPLOYED, LF_LicenseManagerComponent.GetLicenseWhitelistJob(LF_ELicenseType.CRIMINAL_WEED), "CRIMINAL_WEED has no whitelist");
	}
};