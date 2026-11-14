// LF_TestOrgSignAction.c - "Join test organisation" and "Leave organisation" on the playtest sign (#934).
// The base holds the shared behaviour; LF_TestOrgJoinAction and LF_TestOrgLeaveAction are the two
// classes placed on the sign (two classes, so a harness `use` can name one). The server decides
// (LF_TestOrgSign).

class LF_TestOrgSignAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	//! true: join the Test Organisation. false: leave the organisation.
	protected bool Joining()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The sign is placed in the playtest town only. The playtest gate and the dev gate are
	//! server state (LF_SpawnLogic is not on the client), so the server checks them when the
	//! action runs and says why when it refuses.
	override bool CanBeShownScript(IEntity user)
	{
		return user != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		string orgId = LF_OrgReplication.ViewOf(user).m_sOrgId;
		if (Joining() && !orgId.IsEmpty())
		{
			SetCannotPerformReason("#LF-TestOrgSign_AlreadyIn");
			return false;
		}

		if (!Joining() && orgId.IsEmpty())
		{
			SetCannotPerformReason("#LF-TestOrgSign_NotIn");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (Joining())
			outName = "#LF-TestOrgSign_Join";
		else
			outName = "#LF-TestOrgSign_Leave";

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Runs on the server (no HasLocalEffectOnlyScript): the client asks, the server decides.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (!pOwnerEntity || !pUserEntity)
		{
			LF_Debug.Error(LF_TestOrgSign.FEATURE, "sign action performed with no owner or user");
			return;
		}

		if (Joining())
			LF_TestOrgSign.Join(pUserEntity, pOwnerEntity);
		else
			LF_TestOrgSign.Leave(pUserEntity, pOwnerEntity);
	}
}

//------------------------------------------------------------------------------------------------
class LF_TestOrgJoinAction : LF_TestOrgSignAction
{
}

//------------------------------------------------------------------------------------------------
class LF_TestOrgLeaveAction : LF_TestOrgSignAction
{
	override protected bool Joining()
	{
		return false;
	}
}
