//------------------------------------------------------------------------------------------------
//! The recycler bench (#3349): a hands-on station like the chop shop. LOAD takes one item that has a
//! recycle recipe (#3347) out of the worker's inventory, WORK is a held action that adds one unit (so
//! more hands finish sooner), COLLECT hands over the parts the rules give back. One job at a time per
//! bench. The server decides the refusal, the removal and the parts; a client only reads the progress.
//!
//! The item leaves the inventory at LOAD, and a job in progress is not saved (a restart empties the
//! bench), the same as the chop shop. The parts are worth less than the item at catalogue prices
//! (LF_RecycleRules.Problem refuses a recipe that would not lose value), so the bench is no profit loop.
//------------------------------------------------------------------------------------------------
enum LF_ERecycleCheck
{
	NONE,
	TOO_FAR,
	BUSY,
	NO_RECIPE,
	NO_ITEM,
	WRONG_STATE,
	TOO_FAST
}

//------------------------------------------------------------------------------------------------
//! The pure side of the bench: the words a refusal reads in.
class LF_RecyclerRules
{
	static const string FEATURE = "Processing";

	//------------------------------------------------------------------------------------------------
	//! The localization key a worker reads for a refusal; empty for NONE and for a silent one.
	static string RefusalKey(LF_ERecycleCheck check)
	{
		switch (check)
		{
			case LF_ERecycleCheck.TOO_FAR: return "#LF-Recycler_TooFar";
			case LF_ERecycleCheck.BUSY: return "#LF-Recycler_Busy";
			case LF_ERecycleCheck.NO_RECIPE: return "#LF-Recycler_NoRecipe";
			case LF_ERecycleCheck.NO_ITEM: return "#LF-Recycler_NoItem";
			case LF_ERecycleCheck.WRONG_STATE: return "#LF-Recycler_WrongState";
		}

		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Processing", description: "A recycler bench: break an item into part of its parts. Actions: LF_RecyclerAction.")]
class LF_RecyclerComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_RecyclerComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, "Stable id of this bench, for logs")]
	protected string m_sBenchId;

	[Attribute("4", UIWidgets.Auto, "Metres from the bench a worker must stand")]
	protected float m_fReach;

	[Attribute("4", UIWidgets.Auto, "Units of work one job takes; each worker adds one per held action", params: "1 100")]
	protected int m_iWorkUnits;

	[Attribute("1200", UIWidgets.Auto, "Fastest a worker work repeats, in milliseconds")]
	protected int m_iMinTickMs;

	//! -1 empty, 0 to 99 being worked, 100 ready. Server writes, clients read.
	[RplProp()]
	protected int m_iPercent = -1;

	protected ref LF_StationProgress m_Progress = new LF_StationProgress();
	protected ref map<string, float> m_mLastTick = new map<string, float>();
	protected string m_sItemId;
	protected ref array<ref LF_RecycleReturn> m_aReturns = {};

	//------------------------------------------------------------------------------------------------
	string GetBenchId()
	{
		return m_sBenchId;
	}

	//------------------------------------------------------------------------------------------------
	//! -1 empty, 0 to 99 being worked, 100 ready. Same answer on the server and on a client.
	int GetPercent()
	{
		if (Replication.IsServer())
			return m_Progress.Percent();

		return m_iPercent;
	}

	//------------------------------------------------------------------------------------------------
	//! The catalogue id of the item on the bench; empty when the bench is empty (server).
	string GetLoadedItem()
	{
		return m_sItemId;
	}

	//------------------------------------------------------------------------------------------------
	protected void Sync()
	{
		int percent = m_Progress.Percent();
		if (percent == m_iPercent)
			return;

		m_iPercent = percent;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	protected void Reset()
	{
		m_Progress.Clear();
		m_sItemId = string.Empty;
		m_aReturns.Clear();
		m_mLastTick.Clear();
		Sync();
	}

	//------------------------------------------------------------------------------------------------
	protected string WorkerKey(IEntity user)
	{
		string uid = LF_Utils.GetPlayerUID(user);
		if (!uid.IsEmpty())
			return uid;

		return user.GetID().ToString();
	}

	//------------------------------------------------------------------------------------------------
	protected bool InReach(IEntity user)
	{
		return user && vector.Distance(user.GetOrigin(), GetOwner().GetOrigin()) <= m_fReach;
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab of a catalogue item id; empty when the id or its prefab is unknown.
	protected static ResourceName PrefabOf(string itemId)
	{
		LF_CatalogueItem item = LF_ItemCatalogue.Item(LF_ItemCatalogueConfig.Get(), itemId);
		if (!item)
			return ResourceName.Empty;

		return item.m_Prefab;
	}

	//------------------------------------------------------------------------------------------------
	//! The first recipe item the worker carries, or empty when they carry none.
	protected string FindCarriedRecyclable(IEntity user)
	{
		LF_RecycleConfig config = LF_RecycleConfig.Get();
		if (!config || !config.m_aRecipes)
			return string.Empty;

		foreach (LF_RecycleRecipe recipe : config.m_aRecipes)
		{
			if (!recipe)
				continue;

			ResourceName prefab = PrefabOf(recipe.m_sItem);
			if (!prefab.IsEmpty() && LF_InventoryUtils.GetAmount(user, prefab) >= 1)
				return recipe.m_sItem;
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a worker loads the first recyclable item they carry (what the Load action calls).
	LF_ERecycleCheck ServerLoad(IEntity user, float nowSeconds)
	{
		if (!InReach(user))
			return LF_ERecycleCheck.TOO_FAR;

		return ServerLoadItem(user, FindCarriedRecyclable(user), nowSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a worker loads one item by its catalogue id. Nothing is taken until every check passes:
	//! the item needs a recipe that gives something back, resolvable prefabs, and one in the inventory.
	LF_ERecycleCheck ServerLoadItem(IEntity user, string itemId, float nowSeconds)
	{
		// Only the server holds the job and touches inventories; a client call must not change either.
		if (!Replication.IsServer())
			return LF_ERecycleCheck.WRONG_STATE;

		if (!InReach(user))
			return LF_ERecycleCheck.TOO_FAR;

		if (m_Progress.IsLoaded())
			return LF_ERecycleCheck.BUSY;

		LF_RecycleConfig config = LF_RecycleConfig.Get();
		array<ref LF_RecycleReturn> returns = {};
		if (itemId.IsEmpty() || LF_RecycleRules.Yield(config, itemId, returns) < 1)
		{
			LF_Debug.Info(LF_RecyclerRules.FEATURE, string.Format("%1: no recycle recipe for '%2'", m_sBenchId, itemId));
			return LF_ERecycleCheck.NO_RECIPE;
		}

		ResourceName prefab = PrefabOf(itemId);
		if (!Resolves(prefab))
		{
			LF_Debug.Error(LF_RecyclerRules.FEATURE, string.Format("%1: item '%2' has no prefab that resolves, nothing is taken", m_sBenchId, itemId));
			return LF_ERecycleCheck.NO_RECIPE;
		}

		foreach (LF_RecycleReturn line : returns)
		{
			if (!Resolves(PrefabOf(line.m_sItem)))
			{
				LF_Debug.Error(LF_RecyclerRules.FEATURE, string.Format("%1: part '%2' of '%3' has no prefab that resolves, nothing is taken", m_sBenchId, line.m_sItem, itemId));
				return LF_ERecycleCheck.NO_RECIPE;
			}
		}

		if (LF_InventoryUtils.GetAmount(user, prefab) < 1)
		{
			LF_Debug.Info(LF_RecyclerRules.FEATURE, string.Format("%1: %2 does not carry '%3'", m_sBenchId, WorkerKey(user), itemId));
			return LF_ERecycleCheck.NO_ITEM;
		}

		int removed = LF_InventoryUtils.RemoveAmount(user, prefab, 1);
		if (removed != 1)
		{
			if (removed > 0)
				LF_InventoryUtils.AddAmount(user, prefab, removed, true);

			LF_Debug.Warn(LF_RecyclerRules.FEATURE, string.Format("%1: removing '%2' from %3 changed under the load, nothing is taken", m_sBenchId, itemId, WorkerKey(user)));
			return LF_ERecycleCheck.NO_ITEM;
		}

		m_sItemId = itemId;
		m_aReturns = returns;
		m_Progress.Load(LF_Utils.MaxInt(1, m_iWorkUnits));
		Sync();
		LF_Debug.Info(LF_RecyclerRules.FEATURE, string.Format("%1: %2 loaded '%3', %4 part line(s), needs %5 work", m_sBenchId, WorkerKey(user), itemId, returns.Count(), m_Progress.GetNeeded()));
		return LF_ERecycleCheck.NONE;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Resolves(ResourceName prefab)
	{
		if (prefab.IsEmpty())
			return false;

		Resource resource = Resource.Load(prefab);
		return resource && resource.IsValid();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one finished held action adds one unit of work, whoever the worker is.
	LF_ERecycleCheck ServerWork(IEntity user, float nowSeconds)
	{
		// Only the server holds the job and touches inventories; a client call must not change either.
		if (!Replication.IsServer())
			return LF_ERecycleCheck.WRONG_STATE;

		if (!m_Progress.IsLoaded() || m_Progress.IsDone())
			return LF_ERecycleCheck.WRONG_STATE;

		if (!InReach(user))
			return LF_ERecycleCheck.TOO_FAR;

		string key = WorkerKey(user);
		float last = -100000;
		m_mLastTick.Find(key, last);
		if ((nowSeconds - last) * 1000 < m_iMinTickMs)
		{
			LF_Debug.Warn(LF_RecyclerRules.FEATURE, string.Format("%1: work from %2 refused, it came faster than a hold can finish", m_sBenchId, key));
			return LF_ERecycleCheck.TOO_FAST;
		}

		m_mLastTick.Set(key, nowSeconds);
		bool finished = m_Progress.AddWork(1);
		Sync();
		LF_Debug.Info(LF_RecyclerRules.FEATURE, string.Format("%1 worked by %2: %3/%4", m_sBenchId, key, m_Progress.GetWork(), m_Progress.GetNeeded()));
		if (finished)
			LF_Debug.Info(LF_RecyclerRules.FEATURE, string.Format("%1 ready: '%2' is broken down", m_sBenchId, m_sItemId));

		return LF_ERecycleCheck.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a worker takes the parts. They are handed over once; the bench is empty afterwards, so a
	//! second collect is refused. A part that does not fit the inventory drops at the worker's feet.
	LF_ERecycleCheck ServerCollect(IEntity user, float nowSeconds)
	{
		// Only the server holds the job and touches inventories; a client call must not change either.
		if (!Replication.IsServer())
			return LF_ERecycleCheck.WRONG_STATE;

		if (!m_Progress.IsDone())
			return LF_ERecycleCheck.WRONG_STATE;

		if (!InReach(user))
			return LF_ERecycleCheck.TOO_FAR;

		string itemId = m_sItemId;
		array<ref LF_RecycleReturn> returns = m_aReturns;
		// Empty the bench first: whatever happens below, the parts are handed over once.
		m_aReturns = {};
		Reset();

		int given;
		foreach (LF_RecycleReturn line : returns)
		{
			int added = LF_InventoryUtils.AddAmount(user, PrefabOf(line.m_sItem), line.m_iCount, true);
			given += added;
			if (added != line.m_iCount)
				LF_Debug.Error(LF_RecyclerRules.FEATURE, string.Format("%1: only %2 of %3 '%4' could be made for %5", m_sBenchId, added, line.m_iCount, line.m_sItem, WorkerKey(user)));
		}

		LF_Debug.Info(LF_RecyclerRules.FEATURE, string.Format("%1: '%2' collected by %3: %4 part(s) in %5 line(s)", m_sBenchId, itemId, WorkerKey(user), given, returns.Count()));
		return LF_ERecycleCheck.NONE;
	}
}

//------------------------------------------------------------------------------------------------
enum LF_ERecyclerMode
{
	LOAD,
	WORK,
	COLLECT
}

//------------------------------------------------------------------------------------------------
//! The three things a worker does at the bench: LOAD takes an item, WORK is a held action, COLLECT
//! takes the parts.
class LF_RecyclerAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.ComboBox, "What this action does at the bench", "", ParamEnumArray.FromEnum(LF_ERecyclerMode))]
	protected LF_ERecyclerMode m_eMode;

	[Attribute("SOUND_PICK_UP", UIWidgets.EditBox, "Sound event on the bench SCR_SoundDataComponent, played when the action lands")]
	protected string m_sSound;

	protected LF_RecyclerComponent m_Bench;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Bench = LF_Component<LF_RecyclerComponent>.Find(pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled(LF_ProcessAction.SYSTEM) || !m_Bench)
			return false;

		int percent = m_Bench.GetPercent();
		if (m_eMode == LF_ERecyclerMode.LOAD)
			return percent == LF_StationProgress.EMPTY;

		if (m_eMode == LF_ERecyclerMode.WORK)
			return percent >= 0 && percent < LF_StationProgress.READY;

		return percent == LF_StationProgress.READY;
	}

	//------------------------------------------------------------------------------------------------
	override void OnActionStart(IEntity pUserEntity)
	{
		if (m_eMode != LF_ERecyclerMode.WORK || !pUserEntity || pUserEntity != LF_Utils.GetLocalCharacter())
			return;

		CharacterControllerComponent controller = CharacterControllerComponent.Cast(pUserEntity.FindComponent(CharacterControllerComponent));
		if (controller && controller.CanPlayItemGesture())
			controller.TryPlayItemGesture(EItemGesture.EItemGesturePickUp);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses(LF_ProcessAction.SYSTEM, "LF_RecyclerAction"))
			return;

		if (!m_Bench || !pUserEntity)
		{
			LF_Debug.Error(LF_RecyclerRules.FEATURE, "recycler action performed without a bench or a user");
			return;
		}

		if (!Replication.IsServer())
			return;

		float now = GetGame().GetWorld().GetWorldTime() / 1000;
		LF_ERecycleCheck result;
		if (m_eMode == LF_ERecyclerMode.LOAD)
			result = m_Bench.ServerLoad(pUserEntity, now);
		else if (m_eMode == LF_ERecyclerMode.WORK)
			result = m_Bench.ServerWork(pUserEntity, now);
		else
			result = m_Bench.ServerCollect(pUserEntity, now);

		if (result != LF_ERecycleCheck.NONE)
		{
			LF_Debug.Info(LF_RecyclerRules.FEATURE, string.Format("%1 refused %2 (%3)", m_Bench.GetBenchId(), typename.EnumToString(LF_ERecyclerMode, m_eMode), typename.EnumToString(LF_ERecycleCheck, result)));
			string key = LF_RecyclerRules.RefusalKey(result);
			if (!key.IsEmpty())
				LF_Utils.NotifyEntity(pUserEntity, key, "#LF-Recycler_Title", 4.0);

			return;
		}

		if (m_eMode == LF_ERecyclerMode.COLLECT)
			LF_Utils.NotifyEntity(pUserEntity, "#LF-Recycler_Collected", "#LF-Recycler_Title", 4.0);

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(pUserEntity);
		if (character && !m_sSound.IsEmpty())
			character.LF_BroadcastStationSound("LF_RecyclerAction", m_sSound);
	}
}
