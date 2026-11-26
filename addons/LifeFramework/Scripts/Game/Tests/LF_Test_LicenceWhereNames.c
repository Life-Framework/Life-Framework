// red-proof: with `RedProof()` on no licence is looked up, so the "sold", "fee", "key" and "name resolves" checks fail.

//! licence/where-names: which licences a counter sells and how a refusal names one (LF_LicenceWhere; the line text is LF_Test_LicenceWhere). Only the firearm permit and the driving
//! licence are sold at a counter, at the configured fee; a licence resolves from its enum name or its desk name key
//! and nothing else; an unknown or empty name falls back to the neutral line instead of failing.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_LicenceWhereNames : LF_Test
{
	override string GetName()
	{
		return "licence/where-names";
	}

	override void Run(LF_TestContext ctx)
	{
		bool permitSold = false;
		bool drivingSold = false;
		bool unemployedSold = true;
		int permitFee = -1;
		int drivingFee = -1;
		int otherFee = -1;
		string permitKey = "";
		string otherKey = "";
		bool permitByName = false;
		bool drivingByName = false;
		bool deskKeyName = false;
		bool unknownName = true;
		bool emptyName = true;
		LF_ELicenseType permitResolved = LF_ELicenseType.UNEMPLOYED;
		LF_ELicenseType deskResolved = LF_ELicenseType.UNEMPLOYED;
		string soldLine = "";
		string namedLine = "";
		string unknownLine = "x";
		string defaultLine = "y";
		if (!RedProof())
		{
			permitSold = LF_LicenceWhere.IsSold(LF_LicenceDesk.PERMIT);
			drivingSold = LF_LicenceWhere.IsSold(LF_LicenceDesk.DRIVING);
			unemployedSold = LF_LicenceWhere.IsSold(LF_ELicenseType.UNEMPLOYED);
			permitFee = LF_LicenceWhere.Fee(LF_LicenceDesk.PERMIT);
			drivingFee = LF_LicenceWhere.Fee(LF_LicenceDesk.DRIVING);
			otherFee = LF_LicenceWhere.Fee(LF_ELicenseType.UNEMPLOYED);
			permitKey = LF_LicenceWhere.WhereKey(LF_LicenceDesk.PERMIT);
			otherKey = LF_LicenceWhere.WhereKey(LF_ELicenseType.UNEMPLOYED);
			permitByName = LF_LicenceWhere.FromName(typename.EnumToString(LF_ELicenseType, LF_LicenceDesk.PERMIT), permitResolved);
			drivingByName = LF_LicenceWhere.FromName(typename.EnumToString(LF_ELicenseType, LF_LicenceDesk.DRIVING), deskResolved);
			deskKeyName = LF_LicenceWhere.FromName(LF_LicenceDesk.NameKey(LF_LicenceDesk.DRIVING), deskResolved);
			unknownName = LF_LicenceWhere.FromName("not-a-licence", deskResolved);
			emptyName = LF_LicenceWhere.FromName("", deskResolved);
			soldLine = LF_LicenceWhere.Line(LF_LicenceDesk.PERMIT);
			namedLine = LF_LicenceWhere.LineForName(typename.EnumToString(LF_ELicenseType, LF_LicenceDesk.PERMIT));
			unknownLine = LF_LicenceWhere.LineForName("not-a-licence");
			defaultLine = WidgetManager.Translate(LF_LicenceWhere.KEY_DEFAULT);
		}

		ctx.True(permitSold, "the firearm permit is sold at a counter");
		ctx.True(drivingSold, "the driving licence is sold at a counter");
		ctx.False(unemployedSold, "the free starter licence is not sold");
		ctx.Equal(LF_LicencesConfig.PermitFee(), permitFee, "the permit's fee is the licence config's");
		ctx.Equal(LF_LicencesConfig.DriverLicenceFee(), drivingFee, "the driving licence's fee is the licence config's");
		ctx.Equal(0, otherFee, "a licence no counter sells has no fee");
		ctx.EqualStr("#LF-LicenceWhere_FIREARM_PERMIT", permitKey, "a sold licence has its own where key");
		ctx.EqualStr(LF_LicenceWhere.KEY_DEFAULT, otherKey, "a licence no counter sells uses the neutral key");

		ctx.True(permitByName, "a licence resolves from its enum name");
		ctx.Equal(LF_LicenceDesk.PERMIT, permitResolved, "the permit's enum name resolves to the permit");
		ctx.True(drivingByName, "the driving licence resolves from its enum name");
		ctx.True(deskKeyName, "a licence resolves from its desk name key");
		ctx.Equal(LF_LicenceDesk.DRIVING, deskResolved, "the desk name key resolves to the driving licence");
		ctx.False(unknownName, "an unknown name does not resolve");
		ctx.False(emptyName, "an empty name does not resolve");

		ctx.False(soldLine.IsEmpty(), "a sold licence always gets a sentence");
		ctx.EqualStr(soldLine, namedLine, "naming the licence by its enum name gives the same line as the licence itself");
		ctx.EqualStr(defaultLine, unknownLine, "an unknown name falls back to the neutral line");
	}
}
