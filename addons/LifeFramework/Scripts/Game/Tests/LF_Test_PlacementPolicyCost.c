// tier: WORLD
// red-proof: 2026-11-17, `tools/cli test --no-build --red LF_Test_PlacementPolicyCost` printed RED PROOF OK (the RedProof hook installs no cost table, so every kind is free: the dear set places, nothing is charged or refunded); green without the hook (`--only`, with PlacementComposition, PlacementConfirm, PlacementPolicyWho and PlacementPolicyWhere green).
//! placement/policy-cost (#2448): a kind's cost in the placement policy is taken on the server once the
//! spot is VALID, exactly once. A paid set charges once; a refused one (malformed, on top of a set) charges
//! nothing; a player who cannot pay is refused with the reason and keeps their cash; a set whose spawn fails
//! gives the cost back; a refundable set refunds once to the payer on pick-up, and nothing to a bystander
//! who picks it up. Cones on LF_Test_PlacementComposition.STAGE, clear ground away from the fixture spawn point (#2762).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PlacementPolicyCost : LF_Test
{
	protected static const ResourceName MISSING = "{0000000000002448}Prefabs/LF_Test/NoSuchPlacementPiece.et";
	protected static const string PAID = "test-cost-paid";
	protected static const string DEAR = "test-cost-dear";
	protected static const string BROKEN = "test-cost-broken";
	protected static const string CASH_ITEM = "test-cost-cash-item";
	protected static const int PRICE = 50;
	protected static const int WALLET = 200;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "placement/policy-cost";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunJudge(ctx);

		IEntity payer = LF_TestCharacterFixture.Spawn(ctx);
		IEntity bystander = LF_TestCharacterFixture.Spawn(ctx);
		if (!payer || !bystander)
			return;

		LF_PlacementCost.ResetStatics();
		LF_PlacementCompositions.ResetStatics();
		InstallTable();

		vector stage = LF_Test_PlacementComposition.StageGround();
		Stand(payer, stage, 0);
		Stand(bystander, stage + "-1.5 0 0", 0);
		int start = LF_MoneyUtils.GetCash(payer);
		ctx.Equal(WALLET, LF_MoneyUtils.AddCash(payer, WALLET, "test"), "the payer carries their cash");
		int full = start + WALLET;

		RunCharge(ctx, payer, bystander, full);

		LF_PlacementPolicyConfig.Reload();
		LF_PlacementCost.ResetStatics();
		LF_PlacementCompositions.ResetStatics();
		LF_TestCharacterFixture.Delete(payer);
		LF_TestCharacterFixture.Delete(bystander);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: who can pay what.
	protected void RunJudge(LF_TestContext ctx)
	{
		string toast;
		LF_PlacementCostTerms cash = new LF_PlacementCostTerms();
		cash.m_sKind = "Bench";
		cash.m_eCost = LF_EPlacementCost.CASH;
		cash.m_iAmount = 40;
		ctx.EqualStr("", LF_PlacementCost.Judge(cash, 40, toast), "exactly the price pays");
		string why = LF_PlacementCost.Judge(cash, 39, toast);
		ctx.True(why.IndexOf("cannot pay $40") != -1 && why.IndexOf("Bench") != -1, "a dollar short is refused, naming price and kind: " + why);
		ctx.EqualStr(LF_PlacementCost.TOAST_CASH, toast, "with the cash toast");

		LF_PlacementCostTerms item = new LF_PlacementCostTerms();
		item.m_sKind = "Shed";
		item.m_eCost = LF_EPlacementCost.ITEM;
		item.m_iAmount = 2;
		item.m_sItem = "{1111111111111111}Prefabs/Items/Planks.et";
		why = LF_PlacementCost.Judge(item, 1, toast);
		ctx.True(why.IndexOf("needs 2 x Planks") != -1, "one item short is refused, naming the item: " + why);
		ctx.EqualStr(LF_PlacementCost.TOAST_ITEM, toast, "with the item toast");

		LF_PlacementCostTerms free = new LF_PlacementCostTerms();
		free.m_eCost = LF_EPlacementCost.CASH;
		ctx.True(free.IsFree(), "a cash cost of zero is free");
		ctx.EqualStr("", LF_PlacementCost.Judge(free, 0, toast), "and anyone pays it");
		ctx.True(LF_PlacementCost.TermsFor("no-such-kind").IsFree(), "a kind with no entry is free");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCharge(LF_TestContext ctx, IEntity payer, IEntity bystander, int full)
	{
		vector spot[4];
		Math3D.MatrixIdentity4(spot);
		spot[3] = Where(payer) + "0 0 3";

		// A malformed proposal is refused before the cost: nothing is taken.
		vector noForward[4];
		Copy(spot, noForward);
		noForward[2] = vector.Zero;
		ctx.True(Confirm(payer, Cones(PAID), noForward) == null, "a malformed proposal is refused");
		ctx.Equal(full, LF_MoneyUtils.GetCash(payer), "a refused proposal takes nothing");

		// A set they cannot afford is refused with the reason; the cash stays.
		ctx.True(Confirm(payer, Cones(DEAR), spot) == null, "a set the payer cannot afford is refused");
		string refusal = LF_PlacementCompositions.LastRefusal();
		ctx.True(refusal.IndexOf("cannot pay") != -1 && refusal.IndexOf(DEAR) != -1, "the reason says they cannot pay for it: " + refusal);
		ctx.Equal(full, LF_MoneyUtils.GetCash(payer), "the unaffordable set takes nothing");
		ctx.Equal(0, LF_PlacementCompositions.Count(), "and places nothing");

		// A set whose spawn fails is charged after the valid spot and given back at once.
		LF_PlacementComposition broken = Cones(BROKEN);
		broken.m_aPieces.Insert(LF_PlacementPiece.Create(MISSING, "0 0 1.5", 0, "-0.3 0.1 -0.3", "0.3 0.8 0.3"));
		ctx.True(Confirm(payer, broken, spot) == null, "a set whose spawn fails is refused");
		ctx.True(LF_PlacementCompositions.LastRefusal().IndexOf("spawn failed") != -1, "because the spawn failed: " + LF_PlacementCompositions.LastRefusal() + LF_Test_PlacementComposition.DebrisIfBad(LF_PlacementCompositions.LastRefusal().IndexOf("spawn failed") != -1, spot[3]));
		ctx.Equal(full, LF_MoneyUtils.GetCash(payer), "the failed spawn gave the cost back");
		ctx.Equal(0, LF_PlacementCost.Count(), "and remembers no charge");

		// The paid set charges once.
		LF_PlacedComposition placed = Confirm(payer, Cones(PAID), spot);
		if (!placed)
		{
			ctx.Fail("the paid set was refused: " + LF_PlacementCompositions.LastRefusal() + LF_Test_PlacementComposition.DebrisIfBad(false, spot[3]));
			return;
		}

		ctx.Equal(full - PRICE, LF_MoneyUtils.GetCash(payer), "the paid set took the price once");
		ctx.Equal(1, LF_PlacementCost.Count(), "a refundable charge is remembered");

		// A second set on top is refused by the probes: the cost comes after them, so nothing more is taken.
		ctx.True(Confirm(payer, Cones(PAID), spot) == null, "a set on top of the set is refused");
		ctx.Equal(full - PRICE, LF_MoneyUtils.GetCash(payer), "the refused second set took nothing");

		// The payer picks it up: one refund, and a second pick-up refunds nothing.
		BaseWorld world = GetGame().GetWorld();
		IEntity piece = world.FindEntityByID(placed.m_aIds[1]);
		EntityID pieceId = placed.m_aIds[1];
		ctx.Equal(3, LF_PlacementCompositions.PickUp(payer, piece), "the payer picks the set up");
		ctx.Equal(full, LF_MoneyUtils.GetCash(payer), "the pick-up refunded the price");
		ctx.Equal(0, LF_PlacementCost.Count(), "the charge is forgotten");
		ctx.False(LF_PlacementCost.OnPickedUp(payer, pieceId), "a second pick-up of the same set refunds nothing");
		ctx.Equal(full, LF_MoneyUtils.GetCash(payer), "the cash is refunded once");

		// Placed again; a bystander picks it up: the set goes, nobody is refunded.
		placed = Confirm(payer, Cones(PAID), spot);
		if (!placed)
		{
			ctx.Fail("the second paid set was refused: " + LF_PlacementCompositions.LastRefusal());
			return;
		}

		ctx.Equal(full - PRICE, LF_MoneyUtils.GetCash(payer), "the second set took the price");
		int bystanderCash = LF_MoneyUtils.GetCash(bystander);
		ctx.Equal(3, LF_PlacementCompositions.PickUp(bystander, world.FindEntityByID(placed.m_aIds[0])), "the bystander picks the set up");
		ctx.Equal(full - PRICE, LF_MoneyUtils.GetCash(payer), "the payer is not refunded for someone else's pick-up");
		ctx.Equal(bystanderCash, LF_MoneyUtils.GetCash(bystander), "and the bystander gets nothing");
		ctx.Equal(0, LF_PlacementCost.Count(), "the charge is forgotten");

		// A set that goes without a pick-up (a roadblock expiring) leaves no charge behind once pruned.
		placed = Confirm(payer, Cones(PAID), spot);
		if (!placed)
		{
			ctx.Fail("the third paid set was refused: " + LF_PlacementCompositions.LastRefusal());
			return;
		}

		foreach (EntityID id : placed.m_aIds)
			SCR_EntityHelper.DeleteEntityAndChildren(world.FindEntityByID(id));

		ctx.Equal(1, LF_PlacementCost.Prune(), "a charge whose set is gone is forgotten");
		ctx.Equal(0, LF_PlacementCost.Count(), "nothing is left to refund");
		ctx.Equal(full - 2 * PRICE, LF_MoneyUtils.GetCash(payer), "the third set was paid and nothing was refunded for it");

		// Cash named as an item cost is charged as cash, through the ledger; an item cost that is the
		// placed thing itself is caught.
		LF_PlacementCostTerms cashItem = LF_PlacementCost.TermsFor(CASH_ITEM);
		ctx.True(cashItem.m_eCost == LF_EPlacementCost.CASH, "cash as an item cost is charged as cash");
		ctx.Equal(5, cashItem.m_iAmount, "for the same amount");
		LF_PlacementCostTerms kit = new LF_PlacementCostTerms();
		kit.m_eCost = LF_EPlacementCost.ITEM;
		kit.m_iAmount = 1;
		kit.m_sItem = LF_Utils.GetPrefabName(payer);
		ctx.True(LF_PlacementCost.CostIsItself(kit, payer), "an item cost naming the thing placed is caught");
	}

	//------------------------------------------------------------------------------------------------
	//! One confirm with a fresh spawn burst (four a second), so each case reaches the cost, not the limiter.
	protected LF_PlacedComposition Confirm(IEntity player, LF_PlacementComposition composition, vector spot[4])
	{
		LF_RateLimit.ResetStatics();
		return LF_PlacementCompositions.Confirm(player, composition, spot);
	}

	//------------------------------------------------------------------------------------------------
	//! The policy table: a refundable $50 set, a set nobody here can afford, and the set whose spawn fails.
	//! With the red-proof hook on no table is installed, so every kind is free.
	protected void InstallTable()
	{
		if (RedProof())
			return;

		LF_PlacementPolicyConfig config = new LF_PlacementPolicyConfig();
		config.m_aEntries = new array<ref LF_PlacementPolicyEntry>();
		config.m_aEntries.Insert(Entry(PAID, PRICE, true));
		config.m_aEntries.Insert(Entry(DEAR, 1000000, true));
		config.m_aEntries.Insert(Entry(BROKEN, PRICE, true));
		LF_PlacementPolicyEntry cash = Entry(CASH_ITEM, 5, false);
		cash.m_eCost = LF_EPlacementCost.ITEM;
		cash.m_sCostItem = LF_MoneyUtils.CashPrefab();
		config.m_aEntries.Insert(cash);
		LF_PlacementPolicyConfig.UseForTest(config);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PlacementPolicyEntry Entry(string kind, int price, bool refund)
	{
		LF_PlacementPolicyEntry entry = new LF_PlacementPolicyEntry();
		entry.m_sKind = kind;
		entry.m_eCost = LF_EPlacementCost.CASH;
		entry.m_iCostAmount = price;
		entry.m_bRefundOnPickup = refund;
		return entry;
	}

	//------------------------------------------------------------------------------------------------
	//! Three cones across the heading, 1.5 m apart.
	protected LF_PlacementComposition Cones(string name)
	{
		LF_PlacementComposition composition = new LF_PlacementComposition();
		composition.m_sName = name;
		composition.AddPiece(LF_Test_PlacementComposition.CONE, "-1.5 0 0");
		composition.AddPiece(LF_Test_PlacementComposition.CONE, "0 0 0");
		composition.AddPiece(LF_Test_PlacementComposition.CONE, "1.5 0 0");
		return composition;
	}

	//------------------------------------------------------------------------------------------------
	protected vector Where(IEntity character)
	{
		vector transform[4];
		character.GetWorldTransform(transform);
		return transform[3];
	}

	//------------------------------------------------------------------------------------------------
	protected void Copy(vector from[4], out vector to[4])
	{
		for (int i = 0; i < 4; i++)
			to[i] = from[i];
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
