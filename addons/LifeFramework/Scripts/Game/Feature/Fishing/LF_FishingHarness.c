// Harness verb `fishing <fire|state|water x z>` (#2995), for the pond session proof (agent-box/proofs/fishing-proof.mjs).
//   fishing fire         the rod's fire input on this client: strikes when a line is out, else casts at the first water ahead
//   fishing auto         arm a strike for the moment the next bite reaches this client (no harness round trip in the window)
//   fishing state        `line=<0|1> bite=<0|1> float=<none|rest|dip> rod=<mode id=..> auto=<off|armed|fired>`
//   fishing water <x> <z> the water kind at a flat position: OCEAN, POND or NONE (the server's own query)
// Read and drive the local client only; the server decides everything a fish costs or pays.

//------------------------------------------------------------------------------------------------
class LF_FishingHarness
{
	static const int AUTO_POLL_MS = 40;	// lf-fixed: how often an armed strike looks for the bite

	protected static bool s_bAutoArmed;
	protected static bool s_bAutoFired;

	//------------------------------------------------------------------------------------------------
	//! `fishing auto`: strike the moment the bite reaches this client. A harness round trip is slower than
	//! the 2 s window, so the proof arms this instead of polling `state`.
	static void ArmAuto()
	{
		s_bAutoArmed = true;
		s_bAutoFired = false;
		GetGame().GetCallqueue().Remove(AutoTick);
		GetGame().GetCallqueue().CallLater(AutoTick, AUTO_POLL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected static void AutoTick()
	{
		if (!s_bAutoArmed)
		{
			GetGame().GetCallqueue().Remove(AutoTick);
			return;
		}

		if (!LF_FishingRodComponent.HasBite())
			return;

		LF_FishingRodComponent rod = FindRod(LF_Utils.GetLocalCharacter());
		s_bAutoArmed = false;
		s_bAutoFired = true;
		GetGame().GetCallqueue().Remove(AutoTick);
		if (rod)
			LF_FishingRodComponent.Fire(rod.GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	static string AutoState()
	{
		if (s_bAutoArmed)
			return "armed";

		if (s_bAutoFired)
			return "fired";

		return "off";
	}

	//! The rod component of the first carried item that has one, null when the character carries none.
	static LF_FishingRodComponent FindRod(IEntity character)
	{
		if (!character)
			return null;

		// A tool in the hands may not be listed with the carried items: ask the gadget manager first.
		SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(character);
		if (gadgets)
		{
			IEntity heldGadget = gadgets.GetHeldGadget();
			if (heldGadget)
			{
				LF_FishingRodComponent heldRod = LF_FishingRodComponent.Cast(heldGadget.FindComponent(LF_FishingRodComponent));
				if (heldRod)
					return heldRod;
			}
		}

		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(character);
		if (!inventory)
			return null;

		array<IEntity> items = {};
		inventory.GetItems(items);
		foreach (IEntity item : items)
		{
			LF_FishingRodComponent rod = LF_FishingRodComponent.Cast(item.FindComponent(LF_FishingRodComponent));
			if (rod)
				return rod;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "fishing")
			return super.Handle(cmd);

		if (cmd.Arg1 == "fire")
		{
			LF_FishingRodComponent rodComponent = LF_FishingHarness.FindRod(LF_Utils.GetLocalCharacter());
			if (!rodComponent || rodComponent.GetMode() != EGadgetMode.IN_HAND)
			{
				LF_AgentHarness.Answer(cmd.Id, "error", "no_rod_in_hands");
				return true;
			}

			LF_FishingRodComponent.Fire(rodComponent.GetOwner());
			LF_AgentHarness.Answer(cmd.Id, "ok", "fired");
			return true;
		}

		if (cmd.Arg1 == "state")
		{
			string floatState = "none";
			if (LF_FishingFloat.IsDipping())
				floatState = "dip";
			else if (LF_FishingFloat.IsOut())
				floatState = "rest";

			string rodMode = "none";
			LF_FishingRodComponent tool = LF_FishingHarness.FindRod(LF_Utils.GetLocalCharacter());
			if (tool)
				rodMode = typename.EnumToString(EGadgetMode, tool.GetMode()) + " id=" + tool.GetToolId();

			LF_AgentHarness.Answer(cmd.Id, "ok", string.Format("line=%1 bite=%2 float=%3 rod=%4 auto=%5", LF_FishingRodComponent.IsLineOut(), LF_FishingRodComponent.HasBite(), floatState, rodMode, LF_FishingHarness.AutoState()));
			return true;
		}

		if (cmd.Arg1 == "auto")
		{
			LF_FishingHarness.ArmAuto();
			LF_AgentHarness.Answer(cmd.Id, "ok", "armed");
			return true;
		}

		if (cmd.Arg1 == "water")
		{
			vector at = Vector(cmd.Arg2.ToFloat(), 0, cmd.Arg3.ToFloat());
			string kind = LF_FishingSession.WaterKindAt(at);
			if (kind.IsEmpty())
				kind = "NONE";

			LF_AgentHarness.Answer(cmd.Id, "ok", "water=" + kind);
			return true;
		}

		LF_AgentHarness.Answer(cmd.Id, "error", "usage: fishing fire|state|water <x> <z>");
		return true;
	}
}
