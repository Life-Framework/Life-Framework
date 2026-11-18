// red-proof: `tools\cli test --no-build --red LF_Test_StoreAlarm` (RedProof() skips the point where the general store till is told to ring the shop bell, so it keeps the core default kind and the first check fails)
//! Robbery alarm per site: the general store till rings the shop burglar bell, the bank vault keeps its own
//! bell (the pack's LF_BankVault.et names kind bank), and the core default (a till that names no kind) is still the store buzzer. The kind lives
//! on the robbed thing (LF_RobAction.m_sAlarmKind), the sound on the kind in LF_Police.conf.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_StoreAlarm : LF_Test
{
	static const string STORE_TILL = "Grid_shops_till_Till";

	override string GetName()
	{
		return "police/store-alarm";
	}

	override void Run(LF_TestContext ctx)
	{
		BaseWorld world = GetGame().GetWorld();
		IEntity till = world.FindEntityByName(STORE_TILL);
		ctx.NotNull(till, "the shop till is placed in the shops-till grid cell");
		if (!till)
			return;

		LF_RobAction rob = FindRobAction(till);
		ctx.NotNull(rob, "the general store till can be robbed");
		if (!rob)
			return;

		string kind = rob.GetAlarmKind();
		if (RedProof())
			kind = "store";

		ctx.EqualStr("shopbell", kind, "the general store till rings the shop bell");

		string storeEvent = EventOf(ctx, kind);
		string bankEvent = EventOf(ctx, "bank");
		string buzzerEvent = EventOf(ctx, "store");
		ctx.EqualStr("SOUND_LF_ALARM_SHOPBELL_LP", storeEvent, "the general store plays the shop bell event");
		ctx.EqualStr("SOUND_LF_ALARM_BANK_LP", bankEvent, "the bank still plays its own bell event");
		ctx.True(storeEvent != bankEvent, "the store bell is not the bank's");
		ctx.True(storeEvent != buzzerEvent, "the store bell is not the core default buzzer");
		ctx.EqualStr("SOUND_LF_ALARM_STORE_LP", buzzerEvent, "the core default kind (store) still plays the till buzzer");
	}

	//------------------------------------------------------------------------------------------------
	//! The sound event of the normal alarm prefab of a kind, read from the prefab as shipped.
	protected string EventOf(LF_TestContext ctx, string kind)
	{
		ResourceName prefab = LF_PoliceConfig.AlarmPrefabFor(kind, "", false);
		ctx.False(prefab.IsEmpty(), kind + ": an alarm prefab resolves");
		if (prefab.IsEmpty())
			return "";

		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			ctx.Fail(kind + ": the alarm prefab does not load: " + prefab);
			return "";
		}

		IEntitySource source = res.GetResource().ToEntitySource();
		for (int i = 0, count = source.GetComponentCount(); i < count; i++)
		{
			IEntityComponentSource component = source.GetComponent(i);
			if (component.GetClassName() != "LF_SiteAlarmComponent")
				continue;

			ResourceName project;
			string soundEvent;
			component.Get("m_sSoundProject", project);
			component.Get("m_sSoundEvent", soundEvent);
			Resource audio = Resource.Load(project);
			ctx.True(audio && audio.IsValid(), kind + ": the sound project loads: " + project);
			return soundEvent;
		}

		ctx.Fail(kind + ": the prefab has no LF_SiteAlarmComponent: " + prefab);
		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_RobAction FindRobAction(IEntity entity)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(entity.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return null;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction candidate : actions)
		{
			LF_RobAction rob = LF_RobAction.Cast(candidate);
			if (rob)
				return rob;
		}

		return null;
	}
}
