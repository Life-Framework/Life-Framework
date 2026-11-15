//------------------------------------------------------------------------------------------------
//! Harness `pickup [PrefabName]` (#1143): the F-key pickup of the loose item nearest this client's
//! character (a shelf copy in a store), through the vanilla SCR_PickUpItemAction performed on this
//! client exactly as the interaction key performs it: the action's own checks, then its PerformAction,
//! whose inventory manager asks the server to insert the item. Nothing is inserted from script.
//!   PrefabName: only items whose prefab file name contains it (LF_Crackers, WaterBottle).
//! The reply comes once the item sits in the character's inventory, or names the check that refused
//! it: anim_ready (an inventory animation still playing; retried for PICKUP_RETRY_MS), can_insert and
//! the inventory return code (no room), or the action's own cannot-perform reason.
//------------------------------------------------------------------------------------------------
class LF_HarnessPickup
{
	static const string FEATURE = "AgentHarness";
	static const string ACTION = "SCR_PickUpItemAction";
	//! The interaction reach the F key offers an item from, measured from the character's origin.
	// lf-fixed: dev-harness reach, the F key interaction distance
	static const float REACH = 3.0;
	// lf-fixed: dev-harness polling time, not a gameplay tunable
	static const int STEP_MS = 150;
	// lf-fixed: dev-harness polling time, not a gameplay tunable
	static const int PICKUP_RETRY_MS = 3000;
	// lf-fixed: dev-harness polling time, not a gameplay tunable
	static const int INSERT_WAIT_MS = 4000;

	protected static int s_iCommandId;
	protected static string s_sFilter;
	protected static EntityID s_ItemId = EntityID.INVALID;
	protected static ResourceName s_sPrefab;
	protected static float s_fDistance;
	protected static int s_iWaited;
	protected static IEntity s_pBest;
	protected static float s_fBest;
	protected static vector s_vFrom;

	//------------------------------------------------------------------------------------------------
	static void Start(int commandId, string filter)
	{
		GetGame().GetCallqueue().Remove(TryPerform);
		GetGame().GetCallqueue().Remove(WaitInserted);
		s_iCommandId = commandId;
		s_sFilter = filter;
		s_iWaited = 0;
		s_ItemId = EntityID.INVALID;
		TryPerform();
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a prefab path passes the filter (empty takes any).
	static bool Matches(string prefab, string filter)
	{
		if (filter.IsEmpty())
			return true;

		return FilePath.StripExtension(FilePath.StripPath(prefab)).Contains(filter);
	}

	//------------------------------------------------------------------------------------------------
	protected static void TryPerform()
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
		{
			Finish("error", "pickup no_character");
			return;
		}

		IEntity item = Nearest(character);
		if (!item)
		{
			Finish("error", string.Format("pickup none_in_reach filter=%1 reach=%2", s_sFilter, REACH));
			return;
		}

		s_sPrefab = LF_Utils.GetPrefabName(item);
		s_fDistance = vector.Distance(item.GetOrigin(), character.GetOrigin());
		ScriptedUserAction action = ActionOn(item, character);
		if (!action)
		{
			Finish("error", string.Format("pickup cannot_show item=%1", s_sPrefab));
			return;
		}

		if (!action.CanBePerformedScript(character))
		{
			SCR_InventoryStorageManagerComponent manager = SCR_InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));
			bool animReady;
			bool canInsert;
			int code = -1;
			if (manager)
			{
				animReady = manager.IsAnimationReady();
				manager.SetReturnCodeDefault();
				canInsert = manager.CanInsertItem(item, EStoragePurpose.PURPOSE_DEPOSIT);
				code = manager.GetReturnCode();
			}

			// An inventory animation still playing clears in a moment, as it does for a player who
			// presses F again.
			if (manager && !animReady && canInsert && s_iWaited < PICKUP_RETRY_MS)
			{
				s_iWaited += STEP_MS;
				GetGame().GetCallqueue().CallLater(TryPerform, STEP_MS, false);
				return;
			}

			Finish("error", string.Format("pickup cannot_perform item=%1 anim_ready=%2 can_insert=%3 code=%4 reason=%5", s_sPrefab, animReady, canInsert, code, action.GetCannotPerformReason()));
			return;
		}

		s_ItemId = item.GetID();
		action.PerformAction(item, character);
		LF_Debug.Info(FEATURE, string.Format("pickup performed %1 on %2", ACTION, s_sPrefab));
		s_iWaited = 0;
		GetGame().GetCallqueue().CallLater(WaitInserted, STEP_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! The server's insert replicates back: the item then sits in a slot of this character.
	protected static void WaitInserted()
	{
		IEntity item = GetGame().GetWorld().FindEntityByID(s_ItemId);
		InventoryItemComponent slotItem;
		if (item)
			slotItem = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));

		if (slotItem && slotItem.GetParentSlot())
		{
			Finish("ok", string.Format("pickup took item=%1 dist=%2 via=%3", s_sPrefab, s_fDistance, ACTION));
			return;
		}

		s_iWaited += STEP_MS;
		if (s_iWaited >= INSERT_WAIT_MS)
		{
			Finish("error", string.Format("pickup not_inserted item=%1 alive=%2 waited_ms=%3", s_sPrefab, item != null, s_iWaited));
			return;
		}

		GetGame().GetCallqueue().CallLater(WaitInserted, STEP_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Finish(string status, string detail)
	{
		if (status != "ok")
			LF_Debug.Warn(FEATURE, detail);

		LF_AgentHarness.Reply(s_iCommandId, status, detail);
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest loose item in reach whose pickup the F key would show, filter applied.
	protected static IEntity Nearest(IEntity character)
	{
		s_pBest = null;
		s_fBest = REACH;
		s_vFrom = character.GetOrigin();
		GetGame().GetWorld().QueryEntitiesBySphere(s_vFrom, REACH, Collect, null, EQueryEntitiesFlags.ALL);
		return s_pBest;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Collect(IEntity entity)
	{
		if (!entity || entity == LF_Utils.GetLocalCharacter())
			return true;

		InventoryItemComponent slotItem = InventoryItemComponent.Cast(entity.FindComponent(InventoryItemComponent));
		if (!slotItem || slotItem.GetParentSlot())
			return true;

		float distance = vector.Distance(entity.GetOrigin(), s_vFrom);
		if (distance >= s_fBest || !Matches(LF_Utils.GetPrefabName(entity), s_sFilter))
			return true;

		if (!ActionOn(entity, LF_Utils.GetLocalCharacter()))
			return true;

		s_pBest = entity;
		s_fBest = distance;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static ScriptedUserAction ActionOn(IEntity item, IEntity user)
	{
		BaseActionsManagerComponent actions = BaseActionsManagerComponent.Cast(item.FindComponent(BaseActionsManagerComponent));
		if (!actions)
			return null;

		array<BaseUserAction> list = {};
		actions.GetActionsList(list);
		foreach (BaseUserAction action : list)
		{
			ScriptedUserAction scripted = ScriptedUserAction.Cast(action);
			if (scripted && scripted.IsInherited(SCR_PickUpItemAction) && scripted.CanBeShownScript(user))
				return scripted;
		}

		return null;
	}
}
