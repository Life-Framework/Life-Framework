[BaseContainerProps(configRoot: true)]
class EL_ShowIDAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity)
			return;

		// Get the character component from the target entity
		EL_CharacterSurvivalComponent survivalComp = EL_CharacterSurvivalComponent.Cast(pOwnerEntity.FindComponent(EL_CharacterSurvivalComponent));
		if (!survivalComp)
			return;

		// Get the survival stats (which has the character data)
		EL_SurvivalStats stats = survivalComp.GetSurvivalStats();
		if (!stats)
			return;

		// Get the character from the account
		// Assuming the player has an account, get the active character via UID
		EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
		string playerUid = EL_Utils.GetPlayerUid(pUserEntity);
		EL_PlayerAccount account = accountManager.GetAccount(playerUid);
		if (!account)
			return;

		EL_PlayerCharacter character = account.GetActiveCharacter();
		if (!character)
			return;

		// Format the ID info
		string idInfo = string.Format("%1 %2, Age: %3", character.GetFirstName(), character.GetLastName(), character.GetAge().ToString());

		// Show hint
		EL_Utils.Notify(idInfo, "#EL-ID_Information", 10.0);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		// Check if user is looking at another player
		return true; // Basic check, can be refined
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!user)
			return false;

		IEntity owner = GetOwner();
		if (!owner)
			return false;

		// Check distance
		vector userPos = user.GetOrigin();
		vector ownerPos = owner.GetOrigin();
		float distance = vector.Distance(userPos, ownerPos);
		if (distance > 1.5)
			return false;

		// Check if user is facing the owner
		vector userAngles = user.GetAngles();
		vector dirToOwner = ownerPos - userPos;
		dirToOwner.Normalize();

		vector userForward = Vector(userAngles[0], userAngles[1], 0).AnglesToVector();
		userForward[1] = 0; // Ignore pitch
		userForward.Normalize();

		float dotUser = vector.Dot(userForward, dirToOwner);
		// Check if owner is facing the user
		vector ownerAngles = owner.GetAngles();
		vector dirToUser = userPos - ownerPos;
		dirToUser.Normalize();

		vector ownerForward = Vector(ownerAngles[0], ownerAngles[1], 0).AnglesToVector();
		ownerForward[1] = 0;
		ownerForward.Normalize();

		float dotOwner = vector.Dot(ownerForward, dirToUser);

		// Both must be facing each other within ~45 degrees -> dot product > cos(45°) ~= 0.7071
		const float COS_45 = 0.70710678;
		return dotUser > COS_45 && dotOwner > COS_45;
	}
};