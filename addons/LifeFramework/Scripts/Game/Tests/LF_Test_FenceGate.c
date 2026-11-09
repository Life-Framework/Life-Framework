// red-proof: Observed 2026-11-09 with `tools/cli test --no-build --only LF_Test_FenceGate`: changing `total >= threshold` to `total > threshold` in LF_FenceRules.Allows failed `crime/fence-gate` with `- at the threshold the fence sells (expected 0, got 9)`. Restored; green.
// tier: LOGIC
//! The fence gate (#271): below the threshold the purchase is refused, at it the fence sells, no
//! threshold is open, an account-less buyer is refused. The record is the real LF_CrimeRecord.
class LF_Test_FenceGate : LF_Test
{
	protected static const string UID = "fence-gate-a";
	protected static const string TOOL = "{00000000FENCE000}Prefabs/Items/Fence/LF_Drill.et";

	override string GetName()
	{
		return "crime/fence-gate";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_CrimeRecordRow> before = LF_CrimeRecord.ExportAll();
		LF_CrimeRecord.ApplyAll(null);

		ctx.Equal(LF_EShopRefusal.NOT_TRUSTED, LF_FenceRules.Check(UID, 3, TOOL), "a fresh account is refused");

		LF_CrimeRecord.Record(LF_OnCrimeCommitted.Create(UID, LF_WantedReports.OFFENCE_ROBBERY));
		LF_CrimeRecord.Record(LF_OnCrimeCommitted.Create(UID, LF_WantedReports.OFFENCE_VEHICLE_THEFT));
		ctx.Equal(LF_EShopRefusal.NOT_TRUSTED, LF_FenceRules.Check(UID, 3, TOOL), "two jobs of three are still refused");

		LF_CrimeRecord.Record(LF_OnCrimeCommitted.Create(UID, LF_WantedReports.OFFENCE_ROBBERY));
		ctx.Equal(LF_EShopRefusal.NONE, LF_FenceRules.Check(UID, 3, TOOL), "at the threshold the fence sells");
		ctx.Equal(LF_EShopRefusal.NOT_TRUSTED, LF_FenceRules.Check(UID, 4, TOOL), "a higher-tier tool stays closed");
		ctx.Equal(LF_EShopRefusal.NONE, LF_FenceRules.Check(UID, 0, TOOL), "no threshold is open");
		ctx.Equal(LF_EShopRefusal.NOT_TRUSTED, LF_FenceRules.Check("", 1, TOOL), "no account has no record");

		ctx.True(LF_ShopRefusal.Key(LF_EShopRefusal.NOT_TRUSTED) != "", "the refusal has a line to print");
		ctx.Equal(LF_EShopRefusal.NOT_TRUSTED, LF_ShopRefusal.FromWire(LF_EShopRefusal.NOT_TRUSTED), "the refusal survives the wire");

		LF_CrimeRecord.ApplyAll(before);
	}
}
