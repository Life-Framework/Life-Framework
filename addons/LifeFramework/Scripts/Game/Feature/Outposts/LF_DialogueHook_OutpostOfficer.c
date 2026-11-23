//------------------------------------------------------------------------------------------------
//! A condition for the outpost people's officer replies (#4640, follow-up of #4459): "I'm on duty. I'd
//! like to buy." and "I'm with the police." are offered only to an on-duty officer, so nobody else is
//! ever shown a reply that only makes sense in a uniform. It asks the same rule the traders use
//! (LF_PoliceUtils.IsOnDutyCharacter, from the server's account), and the dialogue asks it again when the
//! reply is picked, so a client cannot pick a reply it was never offered. No param; it holds no state.
class LF_DialogueHook_OutpostOfficer : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	override bool Allows(IEntity player, IEntity npc, string param)
	{
		if (!player)
			return false;

		return LF_PoliceUtils.IsOnDutyCharacter(player);
	}
}
