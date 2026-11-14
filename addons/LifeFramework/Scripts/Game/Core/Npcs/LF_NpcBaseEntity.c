[EntityEditorProps(category: "EveronLife/Core/Npcs", description:"General purpose NPC entity")]
class LF_NpcBaseEntityClass : GenericEntityClass
{
}

class LF_NpcBaseEntity : GenericEntity
{
	//! The NPC is stunned (LF_NpcStun, #922). Written by the server; every peer lays the body down
	//! or stands it up from this flag and the rest pose below, so a late joiner sees the same.
	[RplProp(onRplName: "LF_OnStunReplicated")]
	protected bool m_bLF_Stunned;

	//! Where and how the NPC stood when the stun began.
	[RplProp(onRplName: "LF_OnStunReplicated")]
	protected vector m_vLF_RestOrigin;

	[RplProp(onRplName: "LF_OnStunReplicated")]
	protected vector m_vLF_RestAngles;

	//! This peer has laid the body down and owes it a stand-up. Local, never replicated.
	protected bool m_bLF_LyingHere;

	//------------------------------------------------------------------------------------------------
	//! Server: stun or release. The rest pose is taken when a stun begins.
	void LF_SetStunned(bool stunned)
	{
		if (!Replication.IsServer() || stunned == m_bLF_Stunned)
			return;

		if (stunned)
		{
			m_vLF_RestOrigin = GetOrigin();
			m_vLF_RestAngles = GetAngles();
		}

		m_bLF_Stunned = stunned;
		Replication.BumpMe();
		LF_ApplyStunPose();
	}

	//------------------------------------------------------------------------------------------------
	bool LF_IsStunned()
	{
		return m_bLF_Stunned;
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnStunReplicated()
	{
		LF_ApplyStunPose();
	}

	//------------------------------------------------------------------------------------------------
	//! Fall onto the back with the feet where they stood, or stand back up where it was. Idempotent.
	protected void LF_ApplyStunPose()
	{
		if (m_bLF_Stunned)
		{
			vector lying = m_vLF_RestAngles;
			lying[0] = lying[0] + LF_NpcStun.FALL_PITCH;
			vector origin = m_vLF_RestOrigin;
			origin[1] = origin[1] + LF_NpcStun.LIE_LIFT;
			SetAngles(lying);
			SetOrigin(origin);
			m_bLF_LyingHere = true;
			return;
		}

		if (!m_bLF_LyingHere)
			return;

		SetAngles(m_vLF_RestAngles);
		SetOrigin(m_vLF_RestOrigin);
		m_bLF_LyingHere = false;
	}
}
