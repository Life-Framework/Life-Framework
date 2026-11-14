//------------------------------------------------------------------------------------------------
//! What a conversation can ask of a robbery (#936). The fuel attendant's dialogue names these hooks
//! (tools/assets/dialogue/fuel_clerk/dialogue.json); the class name is the registration. Server
//! only; every hook asks LF_RobHold, which checks the weapon, the till, the radius, the cooldown,
//! the police on duty and the faction itself.
//!
//! The same attendant is where paying for fuel will go: a reply with a condition such as
//! "FuelOwed" and an action "FuelPay", written as hooks in the fuel feature, next to these.
//------------------------------------------------------------------------------------------------
//! Condition: "This is a robbery" is offered only to a civilian with a weapon drawn.
class LF_DialogueHook_RobArmed : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	override bool Allows(IEntity player, IEntity npc, string param)
	{
		// No robbery line at all while the Robbery system is off (#960).
		if (!LF_Systems.IsEnabled(LF_RobAction.SYSTEM))
			return false;

		return LF_HoldUp.IsArmed(player) && LF_RobAction.IsFactionAllowedToRob(LF_Utils.GetPlayerUid(player));
	}
}

//------------------------------------------------------------------------------------------------
//! Condition: the robbery can start now (the demand that opens the till).
class LF_DialogueHook_RobReady : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	override bool Allows(IEntity player, IEntity npc, string param)
	{
		return LF_RobHold.Refusal(player, npc).IsEmpty();
	}
}

//------------------------------------------------------------------------------------------------
//! Condition: the robber is armed but the robbery cannot start (the same demand, answered by an
//! empty till and a caption that says why).
class LF_DialogueHook_RobBlocked : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	override bool Allows(IEntity player, IEntity npc, string param)
	{
		return LF_HoldUp.IsArmed(player) && !LF_RobHold.Refusal(player, npc).IsEmpty();
	}
}

//------------------------------------------------------------------------------------------------
//! Action: start the held robbery. A refusal that came up between the offer and the pick is shown
//! as its caption before the attendant's line.
class LF_DialogueHook_RobStart : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	override void Run(IEntity player, IEntity npc, string param, notnull array<string> spoken)
	{
		string refusal = LF_RobHold.Start(player, npc);
		if (!refusal.IsEmpty())
			spoken.Insert(LF_RobHoldRules.RefusalKey(refusal));
	}
}

//------------------------------------------------------------------------------------------------
//! Action: say why the robbery cannot start (a caption; the attendant's "it is empty" follows).
class LF_DialogueHook_RobRefused : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	override void Run(IEntity player, IEntity npc, string param, notnull array<string> spoken)
	{
		string refusal = LF_RobHold.Refusal(player, npc);
		LF_Debug.Info(LF_RobHold.FEATURE, string.Format("robbery demand refused in the dialogue: %1", refusal));
		spoken.Insert(LF_RobHoldRules.RefusalKey(refusal));
	}
}
