// red-proof: removed `steps.Insert(new LF_WorldReset_Evidence());` from LF_InvestigationWorldReset.c and ran
// `tools\cli test --no-build --only LF_Test_WorldReset`; "the evidence step is registered by its own file" and
// "the evidence locker is emptied" failed. Restored it and it passed.

// tier: WORLD
//! The world reset service: every feature's file registers its step through the modded registry
//! (no central list), and a run clears what the feature holds. The crime-only run (the harness
//! `resetcrime`) leaves the vehicles, farm and ore steps out.
class LF_Test_WorldReset : LF_Test
{
	override string GetName()
	{
		return "worldreset/service";
	}

	protected bool HasStep(string name)
	{
		foreach (LF_WorldResetStep step : LF_WorldReset.Steps())
		{
			if (step.GetName() == name)
				return true;
		}

		return false;
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(HasStep("crime"), "the crime step is registered by its own file");
		ctx.True(HasStep("loot"), "the loot step is registered by its own file");
		ctx.True(HasStep("police"), "the police step is registered by its own file");
		ctx.True(HasStep("spikes"), "the spike strip step is registered by its own file");
		ctx.True(HasStep("evidence"), "the evidence step is registered by its own file");
		ctx.True(HasStep("calls"), "the calls step is registered by its own file");
		ctx.True(HasStep("vehicles"), "the vehicles step is registered by its own file");
		ctx.True(HasStep("farm"), "the farm step is registered by its own file");
		ctx.True(HasStep("ore"), "the ore step is registered by its own file");

		LF_EvidenceLocker.ResetStatics();
		LF_EvidenceLocker.LogSeizure("{00000000AAAAAAAA}Prefabs/Items/Test.et", "test room", "uid-test");
		ctx.True(LF_EvidenceLocker.Count() == 1, "one evidence locker entry to clear");

		LF_PhoneCallBacks.Book().Clear();
		LF_PhoneCallBackBook book = LF_PhoneCallBacks.Book();
		ctx.True(book.Clear() == 0, "an empty call-back book clears nothing");

		int runs = LF_WorldReset.RunCount();
		string crimeOnly = LF_WorldReset.RunAll("test", true);
		ctx.True(crimeOnly.Contains("evidence="), "the crime-only run includes the evidence step");
		ctx.False(crimeOnly.Contains("vehicles="), "the crime-only run leaves the vehicles alone");
		ctx.False(crimeOnly.Contains("farm="), "the crime-only run leaves the farm alone");
		ctx.False(crimeOnly.Contains("ore="), "the crime-only run leaves the ore alone");
		ctx.True(LF_EvidenceLocker.Count() == 0, "the evidence locker is emptied");
		ctx.True(LF_WorldReset.RunCount() == runs + 1, "a run is counted");

		// What the world started with stays; what play spilled goes.
		array<ResourceName> bagPrefabs = {LF_Bank.BAG_PREFAB};
		IEntity placedBag = LF_Utils.SpawnEntityPrefab(LF_Bank.BAG_PREFAB, "0 200 0");
		ctx.True(placedBag != null, "a money bag to stand in for a placed one");
		array<EntityID> placed = {};
		LF_WorldReset.RememberPlaced(bagPrefabs, placed);
		IEntity spilledBag = LF_Utils.SpawnEntityPrefab(LF_Bank.BAG_PREFAB, "5 200 0");
		EntityID spilledId = EntityID.INVALID;
		if (spilledBag)
			spilledId = spilledBag.GetID();

		ctx.True(LF_WorldReset.DeleteLoose(bagPrefabs, placed) >= 1, "a spilled bag is removed");
		ctx.True(GetGame().GetWorld().FindEntityByID(spilledId) == null, "the spilled bag is gone");
		ctx.True(placedBag != null && placedBag.GetPrefabData() != null, "the placed bag stays");
		if (placedBag)
			SCR_EntityHelper.DeleteEntityAndChildren(placedBag);

		string full = LF_WorldReset.RunAll("test");
		ctx.True(full.Contains("vehicles=") && full.Contains("farm=") && full.Contains("ore="), "the full run includes vehicles, farm and ore");
	}
}
