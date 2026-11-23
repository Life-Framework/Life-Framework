// red-proof: observed 2026-11-23, `tools\cli test --no-build --red LF_Test_PaperKinds`: RED PROOF OK (the lines, the slip and the hand-over all failed with the hook on); green without it.

//! The three printed papers (WO-W2.6, #4465): a weigh slip, a payslip and a delivery note each resolve a
//! paper prefab on the receipt asset and print the item, amount, price and in-game date; LF_PaperSlip.Give
//! puts the slip in the character's inventory, and the receipt's Read text is the slip. Deletes what it spawns.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PaperKinds : LF_Test
{
	static const string DATE = "05/09/1989 08:30";

	override string GetName()
	{
		return "paper/kinds";
	}

	override void Run(LF_TestContext ctx)
	{
		array<string> kinds = {LF_PaperSlip.KIND_WEIGH, LF_PaperSlip.KIND_PAYSLIP, LF_PaperSlip.KIND_DELIVERY};
		foreach (string kind : kinds)
		{
			ResourceName prefab = LF_PaperSlip.PrefabOf(kind);
			ctx.False(prefab.IsEmpty(), kind + ": a paper prefab is named");
			Resource res = Resource.Load(prefab);
			ctx.True(res && res.IsValid(), kind + ": the paper prefab resolves");
			ctx.True(prefab.Contains("LF_Receipt.et"), kind + ": the paper is the receipt asset's prefab");

			array<string> lines = {};
			if (!RedProof())
				LF_PaperSlip.Lines(kind, "Barley", 3, 120, DATE, lines);

			ctx.Equal(3, lines.Count(), kind + ": three lines print");
			if (lines.Count() == 3)
			{
				ctx.True(lines[1].Contains("Barley"), kind + ": the line names the item (" + lines[1] + ")");
				ctx.True(lines[1].Contains("3"), kind + ": the line has the amount");
				ctx.True(lines[1].Contains("120"), kind + ": the line has the price");
				ctx.True(lines[2].Contains(DATE), kind + ": the in-game date is printed (" + lines[2] + ")");
				ctx.False(lines[0].StartsWith("#"), kind + ": the title is translated");
			}
		}

		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		IEntity slip;
		if (!RedProof())
			slip = LF_PaperSlip.Give(character, LF_PaperSlip.KIND_WEIGH, "Barley", 3, 120);

		ctx.NotNull(slip, "Give makes the weigh slip");
		if (slip)
		{
			InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(character);
			ctx.NotNull(storage, "the character has an inventory");
			if (storage)
				ctx.True(storage.Contains(slip), "the slip is in the character's inventory");

			LF_ReceiptComponent paper = LF_Component<LF_ReceiptComponent>.Find(slip);
			ctx.NotNull(paper, "the slip carries the receipt component");
			if (paper)
			{
				ctx.True(paper.IsPrinted(), "the slip reads as printed");
				ctx.True(paper.Describe().Contains("Barley"), "Read shows the item");
				ctx.True(paper.Describe().Contains("120"), "Read shows the price");
			}

			SCR_EntityHelper.DeleteEntityAndChildren(slip);
		}

		ctx.True(LF_PaperSlipSources.WeighItemFor("grain-sale") != "", "a granary unload is a weigh slip sale");
		ctx.True(LF_PaperSlipSources.WeighItemFor("buyer-sale") != "", "a buyer sale is a weigh slip sale");
		ctx.True(LF_PaperSlipSources.WeighItemFor("store-sale") == "", "a store sale is no weigh slip");

		LF_TestCharacterFixture.Delete(character);
	}
}
