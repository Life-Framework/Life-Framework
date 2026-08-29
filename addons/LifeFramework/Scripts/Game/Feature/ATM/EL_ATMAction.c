[BaseContainerProps(configRoot: true)]
class EL_ATMAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!user)
			return false;

		EL_CharacterATMComponent atmComp = EL_CharacterATMComponent.Cast(user.FindComponent(EL_CharacterATMComponent));
		return atmComp != null;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pUserEntity)
			return;

		EL_CharacterATMComponent atmComp = EL_CharacterATMComponent.Cast(pUserEntity.FindComponent(EL_CharacterATMComponent));
		if (atmComp)
		{
			atmComp.OpenATMMenu();
		}
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!user)
			return false;

		IEntity owner = GetOwner();
		if (!owner)
			return false;

		vector userPos = user.GetOrigin();
		vector ownerPos = owner.GetOrigin();
		float distance = vector.Distance(userPos, ownerPos);
		if (distance > 3.0)
			return false;

		return true;
	}
};
