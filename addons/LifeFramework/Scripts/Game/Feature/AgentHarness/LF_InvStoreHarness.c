// Harness verb `invstore` (playtest round 5, 2026-11-27): the inventory-screen moves a playtest needs
// with furniture storage, a house key and loose items, run on the client the command lands on, through the
// same inventory manager calls the screen makes, so the server's access checks decide:
//   invstore list               the nearest world storage within 4 m and what it holds
//   invstore put <part>         move the first carried item whose prefab path contains <part> into it
//   invstore take <part>        move the first stored item whose prefab path contains <part> into the pack
//   invstore pickup <part>      perform the vanilla pick-up action on the nearest loose matching item (F)
//   invstore aim <part>         start "Place where I aim" on the nearest loose matching placeable; finish
//                               with `spikeaim state|turn|place|cancel`, which drive the same aim mode
//   invstore act <part> <Class> start the named action of a carried item (LF_GangRadioTuneAction)
// Replies `ok ...` or `error <why>`; a move answers at once and lands a moment later (read `list` again).

//------------------------------------------------------------------------------------------------
class LF_InvStoreNear
{
	string m_sPart;
	IEntity m_User;
	vector m_vFrom;
	bool m_bStorage;
	IEntity m_Hit;
	float m_fBest = 1000000;

	//------------------------------------------------------------------------------------------------
	void LF_InvStoreNear(IEntity user, string part, bool storage)
	{
		m_User = user;
		m_vFrom = user.GetOrigin();
		m_sPart = part;
		m_bStorage = storage;
	}

	//------------------------------------------------------------------------------------------------
	bool Collect(IEntity entity)
	{
		if (!entity || entity == m_User || entity.GetRootParent() == m_User || ChimeraCharacter.Cast(entity))
			return true;

		if (m_bStorage)
		{
			if (!LF_InvStoreHarness.StorageOf(entity))
				return true;
		}
		else
		{
			if (entity.GetParent())
				return true;

			InventoryItemComponent item = InventoryItemComponent.Cast(entity.FindComponent(InventoryItemComponent));
			if (!item || item.GetParentSlot())
				return true;

			if (m_sPart != "" && !LF_InvStoreHarness.PrefabOf(entity).Contains(m_sPart))
				return true;
		}

		float d = vector.Distance(entity.GetOrigin(), m_vFrom);
		if (d < m_fBest)
		{
			m_fBest = d;
			m_Hit = entity;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_InvStoreHarness
{
	static const float RADIUS = 4; // lf-fixed: a harness reach, not a game tunable

	//------------------------------------------------------------------------------------------------
	//! The world storage on this entity (furniture, a stash, a crate), or null. A character's own
	//! storages and carried items are skipped by the caller.
	static BaseInventoryStorageComponent StorageOf(IEntity entity)
	{
		if (!entity)
			return null;

		return BaseInventoryStorageComponent.Cast(entity.FindComponent(SCR_UniversalInventoryStorageComponent));
	}

	//------------------------------------------------------------------------------------------------
	static string PrefabOf(IEntity entity)
	{
		if (!entity || !entity.GetPrefabData())
			return "";

		return entity.GetPrefabData().GetPrefabName();
	}

	//------------------------------------------------------------------------------------------------
	static string Contents(BaseInventoryStorageComponent storage)
	{
		array<IEntity> items = {};
		storage.GetAll(items);
		string names;
		foreach (IEntity item : items)
		{
			string prefab = PrefabOf(item);
			int slash = prefab.LastIndexOf("/");
			if (slash >= 0)
				prefab = prefab.Substring(slash + 1, prefab.Length() - slash - 1); // lf-bounded: a prefab path
			if (names != "")
				names += ",";
			names += prefab;
		}

		return string.Format("items=%1 [%2]", items.Count(), names);
	}

	//------------------------------------------------------------------------------------------------
	static string Run(string mode, string part, string cls)
	{
		IEntity user = LF_Utils.GetLocalCharacter();
		if (!user)
			return "error no_character";

		SCR_InventoryStorageManagerComponent manager = SCR_InventoryStorageManagerComponent.Cast(user.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!manager)
			return "error no_inventory";

		if (mode == "pickup" || mode == "aim")
			return Loose(user, mode, part);

		if (mode == "act")
			return ActCarried(user, manager, part, cls);

		LF_InvStoreNear near = new LF_InvStoreNear(user, part, true);
		GetGame().GetWorld().QueryEntitiesBySphere(user.GetOrigin(), RADIUS, near.Collect, null, EQueryEntitiesFlags.ALL);
		BaseInventoryStorageComponent storage = StorageOf(near.m_Hit);
		if (!storage)
			return "error no_storage_within_4m";

		string where = string.Format("storage=%1 dist=%2", PrefabOf(near.m_Hit), near.m_fBest);
		if (mode == "list")
			return string.Format("ok %1 %2", where, Contents(storage));

		if (part == "")
			return "error no_item_named";

		if (mode == "put")
		{
			array<IEntity> carried = {};
			manager.GetItems(carried);
			foreach (IEntity item : carried)
			{
				if (PrefabOf(item).Contains(part))
				{
					bool sent = manager.TryMoveItemToStorage(item, storage);
					return string.Format("ok put %1 sent=%2 %3", PrefabOf(item), sent, where);
				}
			}

			return "error not_carried " + part;
		}

		if (mode == "take")
		{
			array<IEntity> stored = {};
			storage.GetAll(stored);
			foreach (IEntity item : stored)
			{
				if (!PrefabOf(item).Contains(part))
					continue;

				BaseInventoryStorageComponent into = manager.FindStorageForItem(item, EStoragePurpose.PURPOSE_DEPOSIT);
				if (!into)
					return "error no_room_for " + PrefabOf(item);

				bool sent = manager.TryMoveItemToStorage(item, into);
				return string.Format("ok take %1 sent=%2 %3", PrefabOf(item), sent, where);
			}

			return "error not_stored " + part;
		}

		return "error unknown_mode " + mode;
	}

	//------------------------------------------------------------------------------------------------
	//! `act <part> <ActionClass>`: an action of a carried item (the radio's "Tune to gang channel"), started
	//! the way the action menu starts it, so a server-side PerformAction runs on the server.
	protected static string ActCarried(IEntity user, SCR_InventoryStorageManagerComponent manager, string part, string cls)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (!character || part == "" || cls == "")
			return "error need_item_and_action_class";

		array<IEntity> carried = {};
		manager.GetItems(carried);
		foreach (IEntity item : carried)
		{
			if (!PrefabOf(item).Contains(part))
				continue;

			BaseActionsManagerComponent actions = BaseActionsManagerComponent.Cast(item.FindComponent(BaseActionsManagerComponent));
			if (!actions)
				return "error no_actions " + PrefabOf(item);

			array<BaseUserAction> list = {};
			actions.GetActionsList(list);
			foreach (BaseUserAction listed : list)
			{
				ScriptedUserAction action = ScriptedUserAction.Cast(listed);
				if (!action || action.ClassName() != cls)
					continue;

				bool shown = action.CanBeShownScript(user);
				bool perform = action.CanBePerformedScript(user);
				if (!shown || !perform)
					return string.Format("error cannot_perform shown=%1 reason=%2", shown, action.GetCannotPerformReason());

				character.DoStartObjectAction(action);
				character.DoPerformObjectAction(action);
				return string.Format("ok act %1 on %2", cls, PrefabOf(item));
			}

			return "error no_action " + cls;
		}

		return "error not_carried " + part;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Loose(IEntity user, string mode, string part)
	{
		LF_InvStoreNear near = new LF_InvStoreNear(user, part, false);
		GetGame().GetWorld().QueryEntitiesBySphere(user.GetOrigin(), RADIUS, near.Collect, null, EQueryEntitiesFlags.ALL);
		if (!near.m_Hit)
			return "error none_on_ground " + part;

		BaseActionsManagerComponent actions = BaseActionsManagerComponent.Cast(near.m_Hit.FindComponent(BaseActionsManagerComponent));
		if (!actions)
			return "error no_actions " + PrefabOf(near.m_Hit);

		array<BaseUserAction> list = {};
		actions.GetActionsList(list);
		ScriptedUserAction chosen;
		foreach (BaseUserAction listed : list)
		{
			ScriptedUserAction action = ScriptedUserAction.Cast(listed);
			if (chosen || !action)
				continue;

			if (mode == "pickup" && SCR_PickUpItemAction.Cast(action))
				chosen = action;
			else if (mode == "aim" && LF_PlacementAimAction.Cast(action))
				chosen = action;
		}

		if (!chosen)
			return "error no_" + mode + "_action " + PrefabOf(near.m_Hit);

		if (!chosen.CanBeShownScript(user) || !chosen.CanBePerformedScript(user))
			return string.Format("error cannot_perform reason=%1 %2", chosen.GetCannotPerformReason(), PrefabOf(near.m_Hit));

		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (mode == "aim" && character)
		{
			character.DoStartObjectAction(chosen);
			character.DoPerformObjectAction(chosen);
			return string.Format("ok aim %1 running=%2", PrefabOf(near.m_Hit), LF_PlacementAim.IsRunning());
		}

		chosen.PerformAction(near.m_Hit, user);
		return string.Format("ok pickup %1 dist=%2", PrefabOf(near.m_Hit), near.m_fBest);
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "invstore")
			return super.Handle(cmd);

		string reply = LF_InvStoreHarness.Run(cmd.Arg1, cmd.Arg2, cmd.Arg3);
		LF_Debug.Info("InvStore", string.Format("invstore %1 %2: %3", cmd.Arg1, cmd.Arg2, reply));
		if (reply.StartsWith("ok "))
			LF_AgentHarness.Answer(cmd.Id, "ok", reply.Substring(3, reply.Length() - 3)); // lf-bounded: a short reply
		else
			LF_AgentHarness.Answer(cmd.Id, "error", reply);
		return true;
	}
}
