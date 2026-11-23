// Harness verb `quickslot <tool id> [press]` (#4306): reads, on the machine the command lands on (a
// remote client in a session), where this character's hand tool is: carried or not, its 1-0 quick slot,
// its gadget mode, whether it is held. `press` then presses that slot through the vanilla quick bar
// (open, select, close: what a 1-0 key does), so a proof follows the player's path, not LF_HandTool.Equip.
// `ok carried=1 slot=<i> mode=<m> held=<0|1> pressed=<0|1>` or `error not_carried items=<n>`.
// `ground` reads the nearest such tool lying within 6 m and what its vanilla pick-up action says here;
// `pickup` then performs that action as the F key does.

//------------------------------------------------------------------------------------------------
modded class SCR_WeaponSwitchingBaseUI
{
	//------------------------------------------------------------------------------------------------
	//! Presses quick slot `id` as Action_SwitchSlot does, then lets it go. \return false with no quick bar.
	static bool LF_PressQuickSlot(int id)
	{
		if (!s_WeaponSwitchingUI)
			return false;

		if (s_bOpened && s_bRadial)
			s_WeaponSwitchingUI.CloseQuickSlots();

		s_bRadial = false;
		s_WeaponSwitchingUI.OpenQuickSlots();
		SelectQuickSlot(id);
		s_WeaponSwitchingUI.CloseQuickSlots();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "quickslot")
			return super.Handle(cmd);

		IEntity user = LF_Utils.GetLocalCharacter();
		if (!user)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no_character");
			return true;
		}

		if (cmd.Arg2 == "ground" || cmd.Arg2 == "pickup")
		{
			AnswerGround(cmd, user);
			return true;
		}

		IEntity tool = LF_HandTool.Carried(user, cmd.Arg1);
		if (!tool)
		{
			int itemCount;
			LF_HarnessCarries.FindCarried(user, cmd.Arg1, itemCount);
			LF_AgentHarness.Answer(cmd.Id, "error", string.Format("not_carried items=%1 server=%2", itemCount, Replication.IsServer()));
			return true;
		}

		SCR_CharacterInventoryStorageComponent storage = SCR_CharacterInventoryStorageComponent.Cast(user.FindComponent(SCR_CharacterInventoryStorageComponent));
		int slot = -1;
		if (storage)
			slot = storage.GetEntityIndexInQuickslots(tool);

		bool pressed;
		if (cmd.Arg2 == "press" && slot >= 0)
			pressed = SCR_WeaponSwitchingBaseUI.LF_PressQuickSlot(slot);

		string detail = string.Format("carried=1 slot=%1 mode=%2 held=%3 pressed=%4 server=%5", slot, typename.EnumToString(EGadgetMode, LF_HandTool.Of(tool).GetMode()), LF_HandTool.Holds(user, cmd.Arg1), pressed, Replication.IsServer());
		LF_Debug.Info(LF_HandTool.FEATURE, "quickslot " + cmd.Arg1 + " " + detail);
		LF_AgentHarness.Answer(cmd.Id, "ok", detail);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! `quickslot <id> ground|pickup`: the nearest hand tool with this id lying within 6 m, and what its
	//! vanilla pick-up action says to this character here; `pickup` then performs it as the F key does.
	protected void AnswerGround(LF_AgentCommand cmd, IEntity user)
	{
		LF_HandToolNear near = new LF_HandToolNear(cmd.Arg1, user.GetOrigin());
		GetGame().GetWorld().QueryEntitiesBySphere(user.GetOrigin(), 6, near.Collect, null, EQueryEntitiesFlags.ALL);
		if (!near.m_Hit)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "none_on_ground");
			return;
		}

		SCR_PickUpItemAction pick;
		BaseActionsManagerComponent actions = BaseActionsManagerComponent.Cast(near.m_Hit.FindComponent(BaseActionsManagerComponent));
		if (actions)
		{
			array<BaseUserAction> list = {};
			actions.GetActionsList(list);
			foreach (BaseUserAction action : list)
			{
				if (!pick)
					pick = SCR_PickUpItemAction.Cast(action);
			}
		}

		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(user.FindComponent(SCR_InventoryStorageManagerComponent));
		InventoryItemComponent item = InventoryItemComponent.Cast(near.m_Hit.FindComponent(InventoryItemComponent));
		bool fits;
		bool ready;
		if (inventory)
		{
			fits = inventory.CanInsertItem(near.m_Hit, EStoragePurpose.PURPOSE_DEPOSIT);
			ready = inventory.IsAnimationReady();
		}

		bool locked;
		bool slotted;
		if (item)
		{
			locked = item.IsLocked();
			slotted = item.GetParentSlot() != null;
		}

		bool shown;
		bool perform;
		string reason;
		if (pick)
		{
			shown = pick.CanBeShownScript(user);
			perform = pick.CanBePerformedScript(user);
			if (!perform)
				reason = pick.GetCannotPerformReason();
		}

		bool picked;
		if (cmd.Arg2 == "pickup" && pick && shown && perform)
		{
			pick.PerformAction(near.m_Hit, user);
			picked = true;
		}

		string detail = string.Format("dist=%1 action=%2 shown=%3 perform=%4 reason=%5 fits=%6 ready=%7 locked=%8 slotted=%9", near.m_fBest, pick != null, shown, perform, reason, fits, ready, locked, slotted);
		detail += string.Format(" scale=%1 picked=%2 server=%3", near.m_Hit.GetScale(), picked, Replication.IsServer());
		LF_Debug.Info(LF_HandTool.FEATURE, "quickslot ground " + cmd.Arg1 + " " + detail);
		LF_AgentHarness.Answer(cmd.Id, "ok", detail);
	}
}

//------------------------------------------------------------------------------------------------
//! Query collector: the nearest hand tool with one id that no one carries.
class LF_HandToolNear
{
	string m_sId;
	vector m_vFrom;
	IEntity m_Hit;
	float m_fBest = 1000;

	//------------------------------------------------------------------------------------------------
	bool Collect(IEntity entity)
	{
		LF_HandToolComponent tool = LF_HandTool.Of(entity);
		if (!tool || tool.GetToolId() != m_sId || entity.GetRootParent() != entity)
			return true;

		float dist = vector.Distance(entity.GetOrigin(), m_vFrom);
		if (dist < m_fBest)
		{
			m_fBest = dist;
			m_Hit = entity;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	void LF_HandToolNear(string id, vector from)
	{
		m_sId = id;
		m_vFrom = from;
	}
}
