// red-proof: observed 2026-11-18 with `--only` and the LF_SpikeStrips.CapProblem gate in LF_PlacementService.Confirm removed (log tmp/logs/test-20261118-003827.log): FAIL 'a strip past the cap is refused', 'a refused strip places nothing (expected 2, got 3)', 'a refused strip keeps the item'. Restored: green.
// tier: WORLD
//! police/spike-strip-cap (#2416): an officer may have spikeStripMaxPerOfficer strips deployed at
//! once. The next one is refused before anything is spawned, with the reason logged and the item
//! kept; picking one up (or one expiring) frees a place. The strips stand at the officer's feet,
//! laid with the deployer the service would set, so the test needs no valid street spot.
class LF_Test_SpikeStripCap : LF_Test
{
	protected static const string OFFICER_UID = "spike-cap-officer";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/spike-strip-cap";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_SpikeStrips.AtCap(2, 2), "two of two is at the cap");
		ctx.False(LF_SpikeStrips.AtCap(1, 2), "one of two is under it");

		IEntity officer = LF_TestCharacterFixture.Spawn(ctx);
		if (!officer)
			return;

		LF_PlayerAccount account = LF_TestPoliceKit.Account(OFFICER_UID, true);
		account.SetFaction(LF_Faction.POLICE);
		account.SetOnDuty(true);
		LF_PoliceRanks.BindOfficer(officer, OFFICER_UID);

		int cap = LF_PoliceConfig.SpikeStripMaxPerOfficer();
		ctx.True(cap >= 1, "the police config loads a strip cap");

		IEntity item = LF_Utils.SpawnEntityPrefab(LF_Test_PlacementConfirm.ITEM, officer.GetOrigin() + "0.5 0 0");
		LF_PlaceableComponent placeable = LF_Component<LF_PlaceableComponent>.Find(item);
		if (!placeable)
		{
			ctx.Fail("the folded strip has no LF_PlaceableComponent");
			LF_TestCharacterFixture.Delete(item);
			LF_TestCharacterFixture.Delete(officer);
			LF_TestPoliceKit.Release(OFFICER_UID);
			return;
		}

		ctx.EqualStr("", LF_SpikeStrips.CapProblem(officer, placeable), "with no strip down the officer may lay one");

		array<IEntity> strips = {};
		for (int i = 0; i < cap; i++)
		{
			IEntity laid = LF_Utils.SpawnEntityPrefab(LF_SpikeStrips.STRIP_PREFAB, officer.GetOrigin() + Vector(5 + i * 5, 0, 0));
			LF_SpikeStripComponent component = LF_Component<LF_SpikeStripComponent>.Find(laid);
			if (!component)
			{
				ctx.Fail("the strip prefab has no LF_SpikeStripComponent");
				LF_TestCharacterFixture.Delete(laid);
				break;
			}

			component.SetDeployer(OFFICER_UID);
			strips.Insert(laid);
		}

		ctx.Equal(cap, LF_SpikeStripComponent.CountDeployedBy(OFFICER_UID), "the officer's strips are counted by deployer");
		ctx.Equal(0, LF_SpikeStripComponent.CountDeployedBy("someone-else"), "another officer has none down");

		// The next strip is refused by the service, before it spawns anything or uses the item up.
		EntityID itemId = item.GetID();
		int before = LF_SpikeStrips.Count();
		IEntity over = LF_SpikeStrips.Deploy(officer, item);
		ctx.True(over == null, "a strip past the cap is refused");
		ctx.True(LF_SpikeStrips.LastRefusal().Contains("already has"), "the logged reason names the cap: " + LF_SpikeStrips.LastRefusal());
		ctx.Equal(before, LF_SpikeStrips.Count(), "a refused strip places nothing");
		ctx.True(GetGame().GetWorld().FindEntityByID(itemId) != null, "a refused strip keeps the item");
		if (over)
			strips.Insert(over);

		// One picked up: a place is free.
		if (strips.Count() > 0)
		{
			LF_TestCharacterFixture.Delete(strips[0]);
			strips.Remove(0);
		}

		ctx.EqualStr("", LF_SpikeStrips.CapProblem(officer, placeable), "with one picked up the officer may lay another");

		foreach (IEntity strip : strips)
		{
			LF_TestCharacterFixture.Delete(strip);
		}

		LF_TestCharacterFixture.Delete(item);
		LF_TestCharacterFixture.Delete(officer);
		LF_TestPoliceKit.Release(OFFICER_UID);
	}
}
