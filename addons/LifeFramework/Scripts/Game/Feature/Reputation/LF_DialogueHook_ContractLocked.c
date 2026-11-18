//------------------------------------------------------------------------------------------------
//! The other side of LF_DialogueHook_Contract as a condition (#3050): a reply that is offered only
//! when the contract's standing gate does NOT pass yet, so an NPC can answer "come back when people
//! know you" instead of showing a reply that would be refused. Same param (the contract id); a
//! contract nobody defined hides the reply. Allows runs on the server, where the points live.
class LF_DialogueHook_ContractLocked : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	override bool Allows(IEntity player, IEntity npc, string param)
	{
		LF_ReputationContractDef def = LF_ReputationContracts.FindDef(param);
		if (!def || def.Job() == LF_EJobType.UNEMPLOYED)
			return false;

		string uid = LF_Utils.GetPlayerUid(player);
		if (uid.IsEmpty())
			return false;

		return !LF_Reputation.Passes(uid, def.m_sStandingGate);
	}
}
