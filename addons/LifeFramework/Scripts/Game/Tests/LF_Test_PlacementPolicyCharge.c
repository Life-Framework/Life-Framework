// red-proof: pending
// tier: WORLD
//! #1030 acceptance: a paid placement charges once, and only for a valid spot. The spike strip's
//! policy is layered with a price of 40 cash. An officer with 30 cash is refused with the price in the
//! reason and keeps cash and item; with 100 they place the strip and 60 is left; a second strip
//! proposed on the laid one is refused as in the way and costs nothing.
class LF_Test_PlacementPolicyCharge : LF_Test
{
	protected static const string OFFICER_UID = "placement-policy-officer";
	protected static const vector VERGE = "300 0 245.5";
	protected static const int PRICE = 40;

	override string GetName()
	{
		return "placement/policy-charge";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity officer = LF_TestCharacterFixture.Spawn(ctx);
		if (!officer)
			return;

		LF_PlayerAccount account = LF_TestPoliceKit.Account(OFFICER_UID, true);
		account.SetFaction(LF_Faction.POLICE);
		account.SetOnDuty(true);
		LF_PoliceRanks.BindOfficer(officer, OFFICER_UID);

		LF_PlacementPolicyConfig config = new LF_PlacementPolicyConfig();
		config.m_aPolicies = {};
		LF_PlacementPolicy strip = LF_PlacementPolicy.Make("spike_strip");
		strip.m_sJob = "POLICE";
		strip.m_iCash = PRICE;
		config.m_aPolicies.Insert(strip);
		LF_PlacementPolicyConfig.UseForTest(config);

		Stand(officer, VERGE, 40);
		RunCharge(ctx, officer);

		LF_PlacementPolicyConfig.Reload();
		LF_TestCharacterFixture.Delete(officer);
		LF_TestPoliceKit.Release(OFFICER_UID);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCharge(LF_TestContext ctx, IEntity officer)
	{
		IEntity item = LF_Utils.SpawnEntityPrefab(LF_Test_PlacementConfirm.ITEM, officer.GetOrigin() + "0.5 0 0");
		ctx.Equal(30, LF_MoneyUtils.AddCash(officer, 30), "seed 30 cash");

		IEntity placed = LF_PlacementService.ConfirmOwnSpot(officer, item);
		ctx.True(placed == null, "30 cash does not pay 40");
		ctx.True(LF_PlacementService.LastRefusal().IndexOf("policy spike_strip: costs 40 cash, the player has 30") != -1, "the refusal names the price: " + LF_PlacementService.LastRefusal());
		ctx.Equal(30, LF_MoneyUtils.GetCash(officer), "a refused placement takes nothing");
		EntityID itemId = item.GetID();
		ctx.True(GetGame().GetWorld().FindEntityByID(itemId) != null, "a refused placement keeps the item");

		ctx.Equal(70, LF_MoneyUtils.AddCash(officer, 70), "top up to 100");
		placed = LF_PlacementService.ConfirmOwnSpot(officer, item);
		if (!placed)
		{
			ctx.Fail("100 cash pays 40 and the strip is placed: " + LF_PlacementService.LastRefusal());
			LF_TestCharacterFixture.Delete(item);
			return;
		}

		ctx.Equal(100 - PRICE, LF_MoneyUtils.GetCash(officer), "the placement charged once");
		ctx.True(GetGame().GetWorld().FindEntityByID(itemId) == null, "the folded item is used up");

		// A second strip on the laid one: not VALID, so nothing is taken.
		IEntity second = LF_Utils.SpawnEntityPrefab(LF_Test_PlacementConfirm.ITEM, officer.GetOrigin() + "0.5 0 0");
		IEntity again = LF_PlacementService.ConfirmOwnSpot(officer, second);
		ctx.True(again == null, "a strip on the laid strip is refused");
		ctx.True(LF_PlacementService.LastRefusal().IndexOf("in the way") != -1, "the reason says in the way: " + LF_PlacementService.LastRefusal());
		ctx.Equal(100 - PRICE, LF_MoneyUtils.GetCash(officer), "a spot that is not valid costs nothing");

		LF_TestCharacterFixture.Delete(second);
		LF_TestCharacterFixture.Delete(placed);
		LF_MoneyUtils.RemoveCash(officer, LF_MoneyUtils.GetCash(officer));
	}

	//------------------------------------------------------------------------------------------------
	protected void Stand(IEntity character, vector at, float yaw)
	{
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		vector transform[4];
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), transform);
		transform[3] = at;
		character.SetWorldTransform(transform);
		character.Update();
	}
}
