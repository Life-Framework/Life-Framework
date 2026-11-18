// red-proof: red on main, where LF_LicenceWhere does not exist (scripts do not compile); observed with
// `tools\cli test --no-build --only LF_Test_LicenceWhere` before the helper and the refusal hooks were added.
//! Every licence refusal says where to get the licence and what it costs (#3150): the where key and the
//! fee come from LF_LicenceWhere, the shop refusal and the chain gate both carry the line, and a licence
//! with no where text falls back to the neutral core line.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_LicenceWhere : LF_Test
{
	override string GetName()
	{
		return "licence/where";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_LicencesConfig.SetDeskForTest(250, 1);
		LF_LicencesConfig.SetDriverLicenceFeeForTest(120);

		ctx.EqualStr("#LF-LicenceWhere_FIREARM_PERMIT", LF_LicenceWhere.WhereKey(LF_ELicenseType.FIREARM_PERMIT), "the permit has its own where key");
		ctx.EqualStr("#LF-LicenceWhere_DRIVING_LICENCE", LF_LicenceWhere.WhereKey(LF_ELicenseType.DRIVING_LICENCE), "the driving licence has its own where key");
		ctx.EqualStr(LF_LicenceWhere.KEY_DEFAULT, LF_LicenceWhere.WhereKey(LF_ELicenseType.UNEMPLOYED), "a licence with no where text uses the default key");
		ctx.Equal(250, LF_LicenceWhere.Fee(LF_ELicenseType.FIREARM_PERMIT), "the permit fee is the licence config's");
		ctx.Equal(120, LF_LicenceWhere.Fee(LF_ELicenseType.DRIVING_LICENCE), "the driving fee is the licence config's");

		string permit = LF_LicenceWhere.Line(LF_ELicenseType.FIREARM_PERMIT);
		ctx.True(permit.Contains(WidgetManager.Translate(LF_LicenceWhere.WhereKey(LF_ELicenseType.FIREARM_PERMIT))), "the permit line names the place");
		ctx.True(permit.Contains(LF_MoneyFormat.FormatMoney(250)), "the permit line carries the fee");
		ctx.True(LF_LicenceWhere.Line(LF_ELicenseType.DRIVING_LICENCE).Contains(LF_MoneyFormat.FormatMoney(120)), "the driving line carries its fee");

		string fallback = LF_LicenceWhere.Line(LF_ELicenseType.UNEMPLOYED);
		ctx.EqualStr(WidgetManager.Translate(LF_LicenceWhere.KEY_DEFAULT), fallback, "no where text falls back to the neutral line");
		ctx.True(fallback.Length() > 0 && !fallback.Contains("LF-LicenceWhere"), "the fallback is a real sentence");

		string job = LF_LicenceWhere.Line(LF_ELicenseType.MINER_GOLD);
		ctx.True(job != fallback && job.Length() > 0, "a job licence says which job unlocks it");

		string shop = LF_ShopRefusal.Text(LF_EShopRefusal.NEEDS_LICENCE, "FIREARM_PERMIT");
		ctx.True(shop.Contains(permit), "the shop refusal carries the where line");
		ctx.True(LF_ShopRefusal.Text(LF_EShopRefusal.NEEDS_LICENCE, "SOMETHING_UNKNOWN").Contains(fallback), "an unknown licence name still gets the fallback");
		ctx.False(LF_ShopRefusal.Text(LF_EShopRefusal.NOT_ENOUGH_CASH, "FIREARM_PERMIT").Contains(permit), "other refusals do not carry it");

		LF_ChainStepDef step = new LF_ChainStepDef();
		step.m_eLicense = LF_ELicenseType.FIREARM_PERMIT;
		ctx.EqualStr(permit, LF_ChainGate.LicenceWhere(step), "the chain gate refusal carries the where line");
		step.m_eLicense = LF_ELicenseType.UNEMPLOYED;
		ctx.EqualStr("", LF_ChainGate.LicenceWhere(step), "a step with no licence says nothing");

		LF_LicencesConfig.ResetStatics();
	}
}
