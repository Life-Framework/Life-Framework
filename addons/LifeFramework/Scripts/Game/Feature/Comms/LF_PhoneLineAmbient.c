//------------------------------------------------------------------------------------------------
//! The faint line hum of a payphone while its receiver is up: the line is BUSY (the caller is
//! dialling, ringing out or talking). The state is the replicated LF_PhoneLineComponent, so every
//! machine near the phone hears the hum start with the pick-up and stop on hang-up.
[BaseContainerProps()]
class LF_AmbientConditionPhoneLine : LF_AmbientCondition
{
	//------------------------------------------------------------------------------------------------
	override bool IsMet(IEntity owner)
	{
		int talker;
		return HandsetUp(LF_PhoneLineComponent.StateAt(owner, talker));
	}

	//------------------------------------------------------------------------------------------------
	//! A ringing phone has its receiver down; only BUSY means someone holds it.
	static bool HandsetUp(LF_EPhoneLineState state)
	{
		return state == LF_EPhoneLineState.BUSY;
	}
}
