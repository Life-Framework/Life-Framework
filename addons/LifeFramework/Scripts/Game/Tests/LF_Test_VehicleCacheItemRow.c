// red-proof: observed 2026-11-19 `tools/cli test --no-build --only LF_Test_VehicleCacheItemRow` failed on the unfixed row ("a bound house key survives the save row (expected 'house-3435', got '')"), passed with m_sHouseKey on LF_VehicleCacheItemRow.
//! A saved compartment item keeps everything its live record held (#3435 review): a bound house key
//! stashed in a hidden compartment must come back bound after a restart, not as a blank key.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_VehicleCacheItemRow : LF_Test
{
	override string GetName()
	{
		return "vehicles/cache-item-row";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_StorageItemRecord item = LF_StorageItemRecord.Create("{0000000000000000}Prefabs/Test/Key.et", 1, -1, 0, 2);
		item.m_iAmmo = 7;
		item.m_sSerial = "SN-3435";
		item.m_bStolen = true;
		item.m_sHouseKey = "house-3435";

		LF_StorageItemRecord back = LF_VehicleCacheItemRow.From(item).ToItem();

		ctx.EqualStr(item.m_sPrefab, back.m_sPrefab, "the prefab survives the save row");
		ctx.Equal(2, back.m_iSlot, "the slot survives the save row");
		ctx.Equal(7, back.m_iAmmo, "the ammo survives the save row");
		ctx.EqualStr("SN-3435", back.m_sSerial, "the serial survives the save row");
		ctx.True(back.m_bStolen, "the stolen flag survives the save row");
		ctx.EqualStr("house-3435", back.m_sHouseKey, "a bound house key survives the save row");
	}
}
