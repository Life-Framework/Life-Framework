// red-proof: 2026-11-17 `tools\cli test --no-build --red LF_Test_SafeTypes`: RED PROOF OK (RedProof() skips every LF_SafeTypes.Resolve act); failed "the bank's haul beats the box's", "the values the entity left alone come from the type (expected 10, got 0)", "the alarm kind it left alone comes from the type". Green without the hook.
// tier: LOGIC
//! crime/safe-types: the three safe tiers are data. The core table loads with store_box,
//! bank_room and reserve_vault; a type's row fills every value an entity leaves at its default; a
//! value the entity sets itself wins; an unknown or empty id degrades without a VME; and the store
//! box prefab names a type the table has. bank_room is pinned to today's bank numbers, so the
//! town bank does not change when a building moves to it.
class LF_Test_SafeTypes : LF_Test
{
	static const ResourceName STORE_BOX_PREFAB = "{B5A9BA64B3966CC1}Prefabs/Props/Commercial/LF_SafeBox.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "crime/safe-types";
	}

	//------------------------------------------------------------------------------------------------
	//! The core table as shipped, with no pack overlay (the core leg boots without a pack).
	protected LF_SafeTypeConfig LoadCore()
	{
		Resource container = BaseContainerTools.LoadContainer(LF_SafeTypeConfig.CONFIG_PATH);
		if (!container || !container.IsValid() || !container.GetResource())
			return null;

		LF_SafeTypeConfig config = LF_SafeTypeConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (config && !config.m_aTypes)
			config.m_aTypes = new array<ref LF_SafeTypeRow>();

		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_SafeTypeConfig table = LoadCore();
		ctx.NotNull(table, "the core safe type table loads");
		if (!table)
			return;

		RunDefaults(ctx, table);
		RunOverride(ctx, table);
		RunDegrade(ctx, table);
		RunPrefab(ctx, table);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDefaults(LF_TestContext ctx, LF_SafeTypeConfig table)
	{
		// bank_room is today's bank: the numbers LF_BankVault.et sets, so a vault room changes nothing.
		LF_SafeTuning bank = new LF_SafeTuning();
		if (!RedProof())
			ctx.True(LF_SafeTypes.Resolve(table, LF_SafeTypeConfig.TYPE_BANK_ROOM, "test", bank), "bank_room resolves");

		ctx.Equal(1, bank.m_iBagCount, "bank_room: one bag");
		ctx.Equal(500, bank.m_iCashPerBag, "bank_room: 500 a bag");
		ctx.EqualFloat(60, bank.m_fCooldownSeconds, 0.001, "bank_room: 60 s cooldown");
		ctx.Equal(0, bank.m_iMinPoliceOnDuty, "bank_room: no police needed");
		ctx.EqualFloat(0, bank.m_fFuseSeconds, 0.001, "bank_room: no fuse");
		ctx.True(bank.m_sRequiredTools == "drill,charge", "bank_room: drill or charge");
		ctx.True(bank.m_sAlarmKind == "bank", "bank_room: the bank alarm");
		ctx.EqualFloat(0, bank.m_fJamChance, 0.001, "bank_room: the drill never jams");
		ctx.EqualFloat(30, bank.m_fDrillSeconds, 0.001, "bank_room: 30 s drill");
		ctx.False(bank.m_bLoud, "bank_room: not the loud alarm");

		// store_box: a small, quick, toolless haul behind the store alarm, with no police gate.
		LF_SafeTuning box = new LF_SafeTuning();
		if (!RedProof())
			LF_SafeTypes.Resolve(table, LF_SafeTypeConfig.TYPE_STORE_BOX, "test", box);

		ctx.Equal(1, box.m_iBagCount, "store_box: one bag");
		ctx.Equal(150, box.m_iCashPerBag, "store_box: 150 a bag (the shop till payout)");
		ctx.EqualFloat(10, box.m_fFuseSeconds, 0.001, "store_box: a 10 s timer");
		ctx.True(box.m_sRequiredTools.IsEmpty(), "store_box: no tool needed");
		ctx.True(box.m_sAlarmKind == "store", "store_box: the store alarm");
		ctx.Equal(0, box.m_iMinPoliceOnDuty, "store_box: works with no police on duty");
		ctx.EqualFloat(0, box.m_fJamChance, 0.001, "store_box: nothing to jam");
		ctx.True(box.m_sActionName == "#LF-Safe_BoxRob", "store_box: its own action name");

		// reserve_vault: the biggest haul, the longest drill, police required, the loud alarm.
		LF_SafeTuning reserve = new LF_SafeTuning();
		if (!RedProof())
			LF_SafeTypes.Resolve(table, LF_SafeTypeConfig.TYPE_RESERVE_VAULT, "test", reserve);

		ctx.Equal(4, reserve.m_iBagCount, "reserve_vault: four bags");
		ctx.Equal(1000, reserve.m_iCashPerBag, "reserve_vault: 1000 a bag");
		ctx.Equal(2, reserve.m_iMinPoliceOnDuty, "reserve_vault: two police on duty");
		ctx.EqualFloat(90, reserve.m_fDrillSeconds, 0.001, "reserve_vault: 90 s drill");
		ctx.True(reserve.m_bLoud, "reserve_vault: the loud alarm");
		ctx.True(reserve.m_iBagCount * reserve.m_iCashPerBag > bank.m_iBagCount * bank.m_iCashPerBag, "the reserve haul beats the bank's");
		ctx.True(bank.m_iBagCount * bank.m_iCashPerBag > box.m_iBagCount * box.m_iCashPerBag, "the bank's haul beats the box's");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunOverride(LF_TestContext ctx, LF_SafeTypeConfig table)
	{
		// The entity sets its cash, tools, loud flag and action name itself: those win; the rest is the type's.
		LF_SafeTuning own = new LF_SafeTuning();
		own.m_iCashPerBag = 777;
		own.m_sRequiredTools = "keycard";
		own.m_bLoud = true;
		own.m_sActionName = "own action name";
		if (!RedProof())
			LF_SafeTypes.Resolve(table, LF_SafeTypeConfig.TYPE_STORE_BOX, "test", own);

		ctx.Equal(777, own.m_iCashPerBag, "an entity's own cash wins over the type");
		ctx.True(own.m_sRequiredTools == "keycard", "an entity's own tools win over the type");
		ctx.True(own.m_bLoud, "an entity that is loud stays loud under a quiet type");
		ctx.True(own.m_sActionName == "own action name", "an entity's own action name wins");
		ctx.EqualFloat(10, own.m_fFuseSeconds, 0.001, "the values the entity left alone come from the type");
		ctx.True(own.m_sAlarmKind == "store", "the alarm kind it left alone comes from the type");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDegrade(LF_TestContext ctx, LF_SafeTypeConfig table)
	{
		// An unknown id logs LF_Debug.Error and keeps the entity's numbers; it never throws.
		LF_SafeTuning stray = new LF_SafeTuning();
		stray.m_iCashPerBag = 321;
		ctx.False(LF_SafeTypes.Resolve(table, "no_such_safe", "test", stray), "an unknown type id is refused");
		ctx.Equal(321, stray.m_iCashPerBag, "an unknown type id leaves the entity's own numbers");
		ctx.Equal(1, stray.m_iBagCount, "an unknown type id leaves the defaults");

		LF_SafeTuning noTable = new LF_SafeTuning();
		ctx.False(LF_SafeTypes.Resolve(null, LF_SafeTypeConfig.TYPE_BANK_ROOM, "test", noTable), "no table: refused, no crash");
		ctx.Equal(500, noTable.m_iCashPerBag, "no table: the defaults stand");

		// An empty id is the legacy path: nothing changes and nothing is logged.
		LF_SafeTuning legacy = new LF_SafeTuning();
		ctx.True(LF_SafeTypes.Resolve(table, "", "test", legacy), "an empty id is fine");
		ctx.True(legacy.m_sRequiredTools == LF_SafeTuning.DEFAULT_TOOLS, "an empty id changes nothing");
	}

	//------------------------------------------------------------------------------------------------
	//! The store box prefab names a type the table has.
	protected void RunPrefab(LF_TestContext ctx, LF_SafeTypeConfig table)
	{
		string typeId = SafeTypeOf(STORE_BOX_PREFAB);
		ctx.True(typeId == LF_SafeTypeConfig.TYPE_STORE_BOX, "the store box prefab names the store_box type");
		ctx.NotNull(table.Find(typeId), "the type the prefab names is in the table");
	}

	//------------------------------------------------------------------------------------------------
	//! The m_sSafeType a prefab's vault component sets, or empty (also when it has no vault component).
	static string SafeTypeOf(ResourceName prefab)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return "";

		BaseResourceObject object = resource.GetResource();
		if (!object)
			return "";

		BaseContainer entity = object.ToBaseContainer();
		if (!entity)
			return "";

		BaseContainerList components = entity.GetObjectArray("components");
		if (!components)
			return "";

		for (int i = 0; i < components.Count(); i++)
		{
			BaseContainer component = components.Get(i);
			string typeId;
			if (component && component.IsVariableSetDirectly("m_sSafeType") && component.Get("m_sSafeType", typeId))
				return typeId;
		}

		return "";
	}
}
