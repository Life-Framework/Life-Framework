// red-proof: observed 2026-11-18 with `tools/cli test --no-build --only LF_Test_SpikeStripRefund` on main (LF_SpikeStrips.PickUp without the LF_PlacementCost.OnPickedUp call, log tmp/logs/test-20261118-030318.log): FAIL `the pick-up refunded the price (expected 200, got 160)`, `the charge is forgotten (expected 0, got 1)`, `a second pick-up of the same strip refunds nothing`. With the call: green.
//! police/spike-strip-refund (#2502): a spike strip placed under a priced, refundable policy entry
//! gives its charge back once when the officer who paid folds it up, the same as a composition
//! pick-up. A second pick-up of the same strip refunds nothing; another officer folding a paid strip
//! up refunds nobody. The test installs its own policy table (core ships every kind free) and
//! deploys on open grid ground at the annex gate.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_SpikeStripRefund : LF_Test
{
	protected static const string PAYER_UID = "spike-refund-payer";
	protected static const string OTHER_UID = "spike-refund-other";
	protected static const int PRICE = 40;
	protected static const int WALLET = 200;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/spike-strip-refund";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity payer = LF_TestCharacterFixture.Spawn(ctx);
		IEntity other = LF_TestCharacterFixture.Spawn(ctx);
		if (!payer || !other)
			return;

		OnDuty(payer, PAYER_UID);
		OnDuty(other, OTHER_UID);
		// East of the gate's map board, so the strip lands on bare grid ground.
		vector gate = GateGround() + "6 0 0";
		Stand(payer, gate);
		Stand(other, gate + "-3 0 1");
		LF_PlacementCost.ResetStatics();

		IEntity item = LF_Utils.SpawnEntityPrefab(LF_Test_SpikeStrip.ITEM, payer.GetOrigin() + "0.5 0 0");
		LF_PlaceableComponent placeable = LF_Component<LF_PlaceableComponent>.Find(item);
		if (!placeable)
		{
			ctx.Fail("the folded strip has no LF_PlaceableComponent");
			Cleanup(payer, other, item);
			return;
		}

		InstallTable(placeable.GetKind());
		ctx.Equal(PRICE, LF_PlacementCost.TermsFor(placeable.GetKind()).m_iAmount, "the strip kind has the test price");

		ctx.Equal(WALLET, LF_MoneyUtils.AddCash(payer, WALLET, "test"), "the payer carries their cash");
		int full = LF_MoneyUtils.GetCash(payer);

		// Deployed: the price is taken once. Folded up by the payer: given back once.
		IEntity strip = Deploy(payer, item);
		if (!strip)
		{
			ctx.Fail("the paid deploy was refused: " + LF_SpikeStrips.LastRefusal());
			Cleanup(payer, other, item);
			return;
		}

		ctx.Equal(full - PRICE, LF_MoneyUtils.GetCash(payer), "deploying took the price");
		ctx.Equal(1, LF_PlacementCost.Count(), "the refundable charge is remembered");
		EntityID stripId = strip.GetID();
		ctx.True(LF_SpikeStrips.PickUp(payer, strip), "the payer folds the strip up");
		ctx.Equal(full, LF_MoneyUtils.GetCash(payer), "the pick-up refunded the price");
		ctx.Equal(0, LF_PlacementCost.Count(), "the charge is forgotten");
		ctx.False(LF_PlacementCost.OnPickedUp(payer, stripId), "a second pick-up of the same strip refunds nothing");
		ctx.Equal(full, LF_MoneyUtils.GetCash(payer), "the price came back once");

		// Deployed again; another officer folds it up: nobody is refunded.
		IEntity again = LF_Utils.SpawnEntityPrefab(LF_Test_SpikeStrip.ITEM, payer.GetOrigin() + "0.5 0 0");
		strip = Deploy(payer, again);
		if (!strip)
		{
			ctx.Fail("the second paid deploy was refused: " + LF_SpikeStrips.LastRefusal());
			Cleanup(payer, other, again);
			return;
		}

		ctx.Equal(full - PRICE, LF_MoneyUtils.GetCash(payer), "the second strip took the price");
		int otherCash = LF_MoneyUtils.GetCash(other);
		ctx.True(LF_SpikeStrips.PickUp(other, strip), "another officer folds the strip up");
		ctx.Equal(full - PRICE, LF_MoneyUtils.GetCash(payer), "the payer is not refunded for someone else's pick-up");
		ctx.Equal(otherCash, LF_MoneyUtils.GetCash(other), "and the other officer gets nothing");
		ctx.Equal(0, LF_PlacementCost.Count(), "the charge is forgotten");

		Cleanup(payer, other, null);
	}

	//------------------------------------------------------------------------------------------------
	//! One deploy with a fresh spawn burst, so it reaches the cost, not the limiter.
	protected IEntity Deploy(IEntity officer, IEntity item)
	{
		LF_RateLimit.ResetStatics();
		return LF_SpikeStrips.Deploy(officer, item);
	}

	//------------------------------------------------------------------------------------------------
	//! The strip kind priced in cash, refundable on pick-up, police only like the shipped entry.
	protected void InstallTable(string kind)
	{
		LF_PlacementPolicyEntry entry = new LF_PlacementPolicyEntry();
		entry.m_sKind = kind;
		entry.m_eWho = LF_EPlacementWho.ON_DUTY_POLICE;
		entry.m_eCost = LF_EPlacementCost.CASH;
		entry.m_iCostAmount = PRICE;
		entry.m_bRefundOnPickup = true;

		LF_PlacementPolicyConfig config = new LF_PlacementPolicyConfig();
		config.m_aEntries = new array<ref LF_PlacementPolicyEntry>();
		config.m_aEntries.Insert(entry);
		LF_PlacementPolicyConfig.UseForTest(config);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDuty(IEntity character, string uid)
	{
		LF_PlayerAccount account = LF_TestPoliceKit.Account(uid, true);
		account.SetFaction(LF_Faction.POLICE);
		account.SetOnDuty(true);
		LF_PoliceRanks.BindOfficer(character, uid);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity payer, IEntity other, IEntity item)
	{
		LF_TestCharacterFixture.Delete(item);
		LF_PlacementPolicyConfig.Reload();
		LF_PlacementCost.ResetStatics();
		LF_RateLimit.ResetStatics();
		LF_TestCharacterFixture.Delete(payer);
		LF_TestCharacterFixture.Delete(other);
		LF_TestPoliceKit.Release(PAYER_UID);
		LF_TestPoliceKit.Release(OTHER_UID);
	}

	//------------------------------------------------------------------------------------------------
	//! Open grid ground at the annex gate, where LF_Test_PlacementPolicyCost places too.
	protected vector GateGround()
	{
		IEntity spawn = GetGame().GetWorld().FindEntityByName("Spawn_Grid_Gate");
		if (spawn)
			return spawn.GetOrigin();

		return "74 0 108";
	}

	//------------------------------------------------------------------------------------------------
	protected void Stand(IEntity character, vector at)
	{
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		vector transform[4];
		// Facing south, away from the annex fence line north of the gate.
		Math3D.AnglesToMatrix(Vector(180, 0, 0), transform);
		transform[3] = at;
		character.SetWorldTransform(transform);
		character.Update();
	}
}
