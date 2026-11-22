//------------------------------------------------------------------------------------------------
//! A still NPC turns to the player it is talking to (first-2-hours review A8: the town clerk stood
//! with their back to the player through the whole first conversation). Client only and local: the
//! talker's machine turns the body for the length of the scene and puts it back when the scene
//! closes, so nothing is replicated and a bystander's view is untouched. A stunned NPC (lying down)
//! and a seated one keep their pose. Only an LF_NpcBaseEntity turns; a driven character is left alone.
class LF_DialogueFacing
{
	protected static EntityID s_NpcId = EntityID.INVALID;
	//! The body's transform before the turn, one row each (a static fixed array is not used).
	protected static vector s_vRest0;
	protected static vector s_vRest1;
	protected static vector s_vRest2;
	protected static vector s_vRest3;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_NpcId = EntityID.INVALID;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: whether the NPC should turn at all.
	static bool Turns(bool isStillNpc, bool stunned, bool seated)
	{
		return isStillNpc && !stunned && !seated;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: turn the NPC to the player for the scene. \return True when it was turned.
	static bool Begin(IEntity npc, IEntity player)
	{
		End();
		if (!npc || !player)
			return false;

		LF_NpcBaseEntity still = LF_NpcBaseEntity.Cast(npc);
		bool stunned = still && still.LF_IsStunned();
		if (!Turns(still != null, stunned, LF_ResidentActivity.IsSeated(npc)))
			return false;

		vector rest[4];
		npc.GetWorldTransform(rest);
		s_vRest0 = rest[0];
		s_vRest1 = rest[1];
		s_vRest2 = rest[2];
		s_vRest3 = rest[3];
		vector mat[4];
		Math3D.AnglesToMatrix(Vector(LF_DialogueCamera.FaceYaw(npc.GetOrigin(), player.GetOrigin()), 0, 0), mat);
		mat[3] = npc.GetOrigin();
		npc.SetWorldTransform(mat);
		npc.Update();
		s_NpcId = npc.GetID();
		LF_Debug.Info(LF_Dialogue.FEATURE, "the NPC turns to face the player");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: put the NPC back as it stood. Safe when nothing was turned.
	static void End()
	{
		if (s_NpcId == EntityID.INVALID)
			return;

		BaseWorld world = GetGame().GetWorld();
		IEntity npc;
		if (world)
			npc = world.FindEntityByID(s_NpcId);

		s_NpcId = EntityID.INVALID;
		if (!npc)
			return;

		vector mat[4];
		mat[0] = s_vRest0;
		mat[1] = s_vRest1;
		mat[2] = s_vRest2;
		mat[3] = s_vRest3;
		npc.SetWorldTransform(mat);
		npc.Update();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_DialogueFacing's statics (LF_StaticReset finds it by reflection).
class LF_DialogueFacing_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_DialogueFacing";
	}

	override void Apply()
	{
		LF_DialogueFacing.ResetStatics();
	}
}
