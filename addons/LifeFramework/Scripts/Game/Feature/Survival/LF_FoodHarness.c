//------------------------------------------------------------------------------------------------
//! Harness stand-ins for the eat, drink and throw proof (agent-box/proofs/food-proof.mjs). Each one
//! runs the same code path a player's input reaches:
//!   food state         (client) the replicated hunger and thirst, the food in the hands, the life state
//!   food low <n>       hunger and thirst set to n on the server (the debug menu's survival action)
//!   food invuse        (client) the inventory Use on the first food or drink carried: it takes the item
//!                      into the hands and starts the use animation, as the Use entry does
//!   food use           (client) the use input with food in the hands (GadgetActivate)
//!   food throw         (client) the throw key with food in the hands
//!   food thrown        the server's view of the last thrown item: where it lies and how fast it moves
//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "food")
			return super.Handle(cmd);

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no_character");
			return true;
		}

		if (cmd.Arg1 == "state")
		{
			LF_AgentHarness.Answer(cmd.Id, "ok", LF_FoodHarness.StateLine(character));
			return true;
		}

		if (cmd.Arg1 == "near")
		{
			// `food near x,y,z`: this client's own copy of the world around that point.
			LF_AgentHarness.Answer(cmd.Id, "ok", LF_FoodHarness.NearLine(cmd.Arg2));
			return true;
		}

		if (cmd.Arg1 == "low" || cmd.Arg1 == "thrown")
		{
			int value = 0;
			if (!cmd.Arg2.IsEmpty())
				value = cmd.Arg2.ToInt();

			character.LF_AskHarnessFood(cmd.Id, cmd.Arg1, value);
			return true;
		}

		LF_AgentHarness.Answer(cmd.Id, "ok", LF_FoodHarness.Act(character, cmd.Arg1));
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_FoodNearQuery
{
	ref array<IEntity> m_aFound = {};

	//------------------------------------------------------------------------------------------------
	bool Collect(IEntity entity)
	{
		if (entity && entity.FindComponent(SCR_ConsumableItemComponent))
			m_aFound.Insert(entity);

		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_FoodHarness
{
	//------------------------------------------------------------------------------------------------
	//! The food items this machine's world holds within 2 m of "x,y,z", with where each lies.
	static string NearLine(string point)
	{
		array<string> parts = {};
		point.Split(",", parts, true);
		if (parts.Count() < 3)
			return "need x,y,z";

		vector at = Vector(parts[0].ToFloat(), parts[1].ToFloat(), parts[2].ToFloat());
		float radius = 2;
		if (parts.Count() > 3)
			radius = parts[3].ToFloat();

		LF_FoodNearQuery query = new LF_FoodNearQuery();
		GetGame().GetWorld().QueryEntitiesBySphere(at, radius, query.Collect, null, EQueryEntitiesFlags.ALL);
		string line = string.Format("%1 food item(s) near", query.m_aFound.Count());
		foreach (IEntity item : query.m_aFound)
		{
			line += string.Format(" [%1 at %2 visible=%3 parent=%4]", LF_Utils.GetPrefabName(item), item.GetOrigin(), (item.GetFlags() & EntityFlags.VISIBLE) != 0, item.GetParent() != null);
		}

		return line;
	}

	//------------------------------------------------------------------------------------------------
	static string StateLine(SCR_ChimeraCharacter character)
	{
		string hunger = "?";
		string thirst = "?";
		LF_CharacterSurvivalComponent survival = LF_CharacterSurvivalComponent.Cast(character.FindComponent(LF_CharacterSurvivalComponent));
		if (survival)
		{
			hunger = survival.GetReplicatedHunger().ToString();
			thirst = survival.GetReplicatedThirst().ToString();
		}

		string held = "none";
		IEntity food = LF_FoodThrow.HeldFood(character);
		if (food)
			held = LF_Utils.GetPrefabName(food);

		return string.Format("hunger=%1 thirst=%2 held=%3 life=%4 refusal=%5", hunger, thirst, held, LF_AgentHarness.LifeStateToken(character), LF_ConsumeRules.RefusalFor(character));
	}

	//------------------------------------------------------------------------------------------------
	//! The first food or drink the character carries, or null.
	static IEntity CarriedFood(SCR_ChimeraCharacter character)
	{
		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(character);
		if (!inventory)
			return null;

		array<IEntity> items = {};
		inventory.GetItems(items);
		foreach (IEntity item : items)
		{
			if (LF_FoodThrow.IsFood(item))
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: `invuse`, `use` or `throw`; the reply line says what happened.
	static string Act(SCR_ChimeraCharacter character, string what)
	{
		if (what == "throw")
		{
			LF_FoodThrow.Ask();
			return "throw asked, held=" + LF_Utils.GetPrefabName(LF_FoodThrow.HeldFood(character));
		}

		if (what == "invuse")
		{
			IEntity carried = CarriedFood(character);
			if (!carried)
				return "no food carried";

			SCR_ConsumableItemComponent consumable = SCR_ConsumableItemComponent.Cast(carried.FindComponent(SCR_ConsumableItemComponent));
			bool handled = consumable && consumable.HandleInventoryUse(character);
			return string.Format("inventory Use on %1 handled=%2", LF_Utils.GetPrefabName(carried), handled);
		}

		if (what == "use")
		{
			IEntity held = LF_FoodThrow.HeldFood(character);
			if (!held)
				return "no food held";

			SCR_ConsumableItemComponent heldConsumable = SCR_ConsumableItemComponent.Cast(held.FindComponent(SCR_ConsumableItemComponent));
			heldConsumable.ActivateAction();
			return "use activated on " + LF_Utils.GetPrefabName(held);
		}

		return "unknown food action " + what;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	//! Harness `food low <n>` / `food thrown`: the server side, behind the harness dev gate.
	void LF_AskHarnessFood(int commandId, string what, int value)
	{
		if (Replication.IsServer())
		{
			LF_ApplyHarnessFood(commandId, LF_DevGate.CheckHarnessRequest(this, "food " + what), what, value);
			return;
		}

		Rpc(RpcAsk_LF_HarnessFood, commandId, what, value);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessFood(int commandId, string what, int value)
	{
		LF_ApplyHarnessFood(commandId, LF_DevGate.CheckHarnessRequest(this, "food " + what), what, value);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_ApplyHarnessFood(int commandId, bool gateOpen, string what, int value)
	{
		if (!gateOpen)
		{
			LF_HarnessReply(commandId, "error", LF_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		if (what == "low")
		{
			LF_DebugActionUtils.ApplySurvival(this, value);
			LF_HarnessReply(commandId, "ok", string.Format("survival set to %1", value));
			return;
		}

		IEntity thrown = LF_FoodThrow.LastThrown();
		if (!thrown)
		{
			LF_HarnessReply(commandId, "error", "nothing thrown");
			return;
		}

		float speed = 0;
		Physics physics = thrown.GetPhysics();
		if (physics)
			speed = physics.GetVelocity().Length();

		vector at = thrown.GetOrigin();
		LF_HarnessReply(commandId, "ok", string.Format("thrown %1 at %2 speed=%3 from thrower %4 m ground=%5 visible=%6 parent=%7", LF_Utils.GetPrefabName(thrown), at, speed, vector.Distance(at, GetOrigin()), GetGame().GetWorld().GetSurfaceY(at[0], at[2]), (thrown.GetFlags() & EntityFlags.VISIBLE) != 0, thrown.GetParent() != null));
	}
}
