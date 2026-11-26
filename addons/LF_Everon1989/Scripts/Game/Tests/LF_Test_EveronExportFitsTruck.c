// red-proof: 2026-11-27 (flows-1127): RedProof() skips the loading, so "the cargo truck's storage takes every unit of <kind>" fails for every kind (0 of 40, 20, 100, 120); the green run loads the real counts (40 iron bars, 20 copper, 100 coal, 120 planks all fit). A first hook that doubled the counts was hollow: the truck's storage holds 240 planks too.
//! Every export order the pack opens must fit in the truck a hauler is told to bring (the 2026-11-26 round: "the order
//! wanted 120 planks and the harness cannot put them in the pockets"). The hand-in counts the goods in a vehicle backed
//! up to the crane spot and in the pockets, so an order a Cargo truck's storage cannot hold is an order no player can
//! finish. The test puts each template's whole count into a spawned Cargo truck's storage with no overflow drop.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronExportFitsTruck : LF_Test
{
	static const ResourceName CARGO_TRUCK = "{E1A923BE1A000001}Prefabs/Vehicles/Wheeled/M923A1/EL_M923A1_transport.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/export-fits-truck";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ExportOrderConfig.Reload();
		LF_ExportOrderConfig config = LF_ExportOrderConfig.Get();
		ctx.True(config && config.m_aTemplates.Count() > 0, "the pack opens export orders");
		if (!config)
			return;

		IEntity carrier = LF_TestCharacterFixture.Spawn(ctx);
		if (!carrier)
			return;

		foreach (LF_ExportOrderTemplate template : config.m_aTemplates)
			CheckTemplate(ctx, carrier, template);

		LF_TestCharacterFixture.Delete(carrier);
		LF_ExportOrderConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckTemplate(LF_TestContext ctx, IEntity carrier, LF_ExportOrderTemplate template)
	{
		IEntity truck = LF_Utils.SpawnEntityPrefab(CARGO_TRUCK, carrier.GetOrigin() + "0 0 8");
		ctx.NotNull(truck, "the cargo truck spawns for " + template.m_sId);
		if (!truck)
			return;

		InventoryStorageManagerComponent storage = LF_FeedFromVehicle.StorageOf(truck);
		ctx.NotNull(storage, "the cargo truck has a storage manager");
		if (storage)
		{
			int want = template.m_iCount;
			int put;
			if (!RedProof())
				put = LF_InventoryUtils.AddAmount(storage, template.m_Goods, want, false);

			LF_Debug.Info("ExportOrder", string.Format("fit check %1: %2 of %3 %4 went into the cargo truck", template.m_sId, put, want, template.m_sGoodsName));
			ctx.Equal(want, put, "the cargo truck's storage takes every unit of " + template.m_sId + " (" + want + " " + template.m_sGoodsName + ")");
			ctx.Equal(put, LF_InventoryUtils.GetAmount(storage, template.m_Goods), "the hand-in would count what went in: " + template.m_sId);
		}

		SCR_EntityHelper.DeleteEntityAndChildren(truck);
	}
}
