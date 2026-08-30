// red-proof: change GetLicenseWhitelistJob to return POLICE for MEDIC_ACCESS
// (or drop a POLICE_* case) and run `tools\cli test --tier fast`; the mapping
// assertions fail.
// tier: LOGIC
class EL_Test_LicenseWhitelist : EL_Test
{
	override string GetName()
	{
		return "license/whitelist-job-mapping";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.Equal(EL_EJobType.POLICE, EL_LicenseManagerComponent.GetLicenseWhitelistJob(EL_ELicenseType.POLICE_ACCESS), "POLICE_ACCESS gates on the POLICE whitelist");
		ctx.Equal(EL_EJobType.POLICE, EL_LicenseManagerComponent.GetLicenseWhitelistJob(EL_ELicenseType.POLICE_ARREST), "POLICE_ARREST gates on the POLICE whitelist");
		ctx.Equal(EL_EJobType.POLICE, EL_LicenseManagerComponent.GetLicenseWhitelistJob(EL_ELicenseType.POLICE_FINE), "POLICE_FINE gates on the POLICE whitelist");
		ctx.Equal(EL_EJobType.POLICE, EL_LicenseManagerComponent.GetLicenseWhitelistJob(EL_ELicenseType.POLICE_SEARCH), "POLICE_SEARCH gates on the POLICE whitelist");

		ctx.Equal(EL_EJobType.MEDIC, EL_LicenseManagerComponent.GetLicenseWhitelistJob(EL_ELicenseType.MEDIC_ACCESS), "MEDIC_ACCESS gates on the MEDIC whitelist, not POLICE");

		ctx.Equal(EL_EJobType.UNEMPLOYED, EL_LicenseManagerComponent.GetLicenseWhitelistJob(EL_ELicenseType.FARMER_APPLE), "FARMER_APPLE has no whitelist");
		ctx.Equal(EL_EJobType.UNEMPLOYED, EL_LicenseManagerComponent.GetLicenseWhitelistJob(EL_ELicenseType.CRIMINAL_WEED), "CRIMINAL_WEED has no whitelist");
	}
};