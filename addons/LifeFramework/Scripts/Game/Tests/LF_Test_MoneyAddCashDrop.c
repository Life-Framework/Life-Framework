// red-proof: 2026-11-18 `tools\cli test --no-build --red LF_Test_MoneyAddCashDrop`: RED PROOF OK; the hook calls AddCash without the flag (main's behaviour): `- with the flag the full amount counts as added (expected 250, got 0)`, `- the ledger hears the add once (expected 1, got 0)`. Green with the flag.
// tier: WORLD
//! LF_MoneyUtils.AddCash's opt-in drop-overflow (#2503): cash that does not fit in the inventory lies
//! at the target's feet, counts as added, and the ledger hears the full amount once. Without the flag
//! nothing changes: a full inventory takes nothing and nothing lands on the ground.
class LF_Test_MoneyAddCashDrop : LF_Test
{
	protected static const string UID = "test-addcash-drop";
	protected static const int AMOUNT = 250;
	protected static const float NEAR = 2;
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";

	protected static int s_iGroundCash;

	override string GetName()
	{
		return "money/addcash-drop";
	}

	override void Run(LF_TestContext ctx)
	{
		// No jacket: the body has no pocket.
		IEntity body = LF_ExploitGuardsWorldFixture.SpawnCharacter(ctx, "98 0 94", false);
		if (!body)
			return;

		// The bare body's only free slot for a new item is the hand slot: an apple fills it.
		IEntity apple = LF_Utils.SpawnEntityPrefab(APPLE, body.GetOrigin());
		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(body);
		ctx.True(apple && storage && storage.TryInsertItem(apple), "an apple fills the body's hand slot");

		LF_MoneyUtils.SetUidForTests(body, UID);
		LF_MoneyMovedRecorder recorder = new LF_MoneyMovedRecorder();
		recorder.m_sUid = UID;
		LF_EventBus.Subscribe(LF_OnMoneyMoved, recorder);

		int carried = LF_MoneyUtils.GetCash(body);
		ctx.Equal(0, Math.Max(carried, 0), "the pocketless body carries no cash");

		// Default call: exactly today's behaviour.
		ctx.Equal(0, LF_MoneyUtils.AddCash(body, AMOUNT, "test"), "without the flag a full inventory takes nothing");
		ctx.Equal(0, GroundCashNear(body), "without the flag nothing lands on the ground");
		ctx.Equal(0, recorder.m_iCount, "without the flag the ledger hears nothing");

		bool drop = !RedProof();
		ctx.Equal(AMOUNT, LF_MoneyUtils.AddCash(body, AMOUNT, "test", "", drop), "with the flag the full amount counts as added");
		ctx.Equal(AMOUNT, GroundCashNear(body), "the cash that did not fit lies within 2 m of the body");
		ctx.Equal(1, recorder.m_iCount, "the ledger hears the add once");
		ctx.Equal(AMOUNT, recorder.m_iAmount, "the ledger records the full amount");
		ctx.True(recorder.m_bCredit, "the ledger records a credit");

		LF_EventBus.Unsubscribe(LF_OnMoneyMoved, recorder);
		LF_MoneyUtils.SetUidForTests(null, "");
		ClearGroundCash(body);
		if (apple && LF_InventoryUtils.GetStorageHierachyRoot(apple) == apple)
			SCR_EntityHelper.DeleteEntityAndChildren(apple);

		SCR_EntityHelper.DeleteEntityAndChildren(body);
	}

	//------------------------------------------------------------------------------------------------
	//! The cash lying loose (not in any inventory) within NEAR of `body`.
	protected static int GroundCashNear(IEntity body)
	{
		s_iGroundCash = 0;
		GetGame().GetWorld().QueryEntitiesBySphere(body.GetOrigin(), NEAR, CountGroundCash, null, EQueryEntitiesFlags.ALL);
		return s_iGroundCash;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CountGroundCash(IEntity entity)
	{
		if (!IsLooseCash(entity))
			return true;

		LF_QuantityComponent quantity = LF_Component<LF_QuantityComponent>.Find(entity);
		if (quantity)
			s_iGroundCash += quantity.GetQuantity();
		else
			s_iGroundCash++;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsLooseCash(IEntity entity)
	{
		return entity && LF_Utils.SamePrefab(LF_Utils.GetPrefabName(entity), LF_MoneyUtils.CashPrefab()) && LF_InventoryUtils.GetStorageHierachyRoot(entity) == entity;
	}

	protected static ref array<EntityID> s_aLoose;

	//------------------------------------------------------------------------------------------------
	protected static void ClearGroundCash(IEntity body)
	{
		s_aLoose = {};
		GetGame().GetWorld().QueryEntitiesBySphere(body.GetOrigin(), NEAR, CollectGroundCash, null, EQueryEntitiesFlags.ALL);
		foreach (EntityID loose : s_aLoose)
			SCR_EntityHelper.DeleteEntityAndChildren(GetGame().GetWorld().FindEntityByID(loose));

		s_aLoose = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CollectGroundCash(IEntity entity)
	{
		if (IsLooseCash(entity))
			s_aLoose.Insert(entity.GetID());

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Counts the cash credits LF_OnMoneyMoved reports for one uid.
class LF_MoneyMovedRecorder : LF_EventSubscriber
{
	string m_sUid;
	int m_iCount;
	int m_iAmount;
	bool m_bCredit;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnMoneyMoved moved = LF_OnMoneyMoved.Cast(busEvent);
		if (!moved || moved.m_sPlayerUid != m_sUid || moved.m_sAccount != LF_Ledger.ACCOUNT_CASH)
			return true;

		m_iCount++;
		m_iAmount += moved.m_iAmount;
		m_bCredit = moved.m_bCredit;
		return true;
	}
}
