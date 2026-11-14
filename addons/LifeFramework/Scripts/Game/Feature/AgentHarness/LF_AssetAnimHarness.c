//------------------------------------------------------------------------------------------------
//! Harness stand-ins for the asset animation proof (#340, agent-box/proofs/drill-scene-proof.mjs).
//! The vault drill runs for six seconds, too short to join a second client into, so:
//!   animhold  the server spawns a drill held in the air in front of the caller, switched on, and
//!             leaves it running
//!   animoff   the server switches that drill off and deletes it
//!   animseen  (client) what this machine shows of the nearest animated asset: the replicated state,
//!             the effects and sounds running here, and the first moving bone's rotation, which
//!             differs between two calls while the part spins
//------------------------------------------------------------------------------------------------
class LF_AssetAnimHarness
{
	protected static EntityID s_Held;

	//------------------------------------------------------------------------------------------------
	static string Hold(IEntity character)
	{
		if (!character)
			return "error no_character";

		Off();
		vector transform[4];
		character.GetWorldTransform(transform);
		// One step ahead of where a robber's own drill is held, so it is not inside the caller's hands.
		vector at = LF_DrillScene.HoldPosition(transform[3], transform[2]) + transform[2] * 0.8;
		IEntity drill = LF_Utils.SpawnEntityPrefab(LF_Bank.DRILL_PREFAB, at);
		LF_AssetAnimComponent anim = LF_AssetAnimComponent.Of(drill);
		if (!anim)
		{
			if (drill)
				SCR_EntityHelper.DeleteEntityAndChildren(drill);

			return "error the_drill_has_no_asset_anim_component";
		}

		anim.Pose(at, Vector(0, character.GetAngles()[1], 0));
		anim.SetActive(true);
		s_Held = drill.GetID();
		return string.Format("ok held at %1 running=%2", at, anim.IsRunning());
	}

	//------------------------------------------------------------------------------------------------
	static string Off()
	{
		IEntity drill = GetGame().GetWorld().FindEntityByID(s_Held);
		s_Held = EntityID.INVALID;
		if (!drill)
			return "ok nothing_held";

		LF_AssetAnimComponent anim = LF_AssetAnimComponent.Of(drill);
		if (anim)
			anim.SetActive(false);

		SCR_EntityHelper.DeleteEntityAndChildren(drill);
		return "ok off";
	}

	//------------------------------------------------------------------------------------------------
	static string Seen()
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
			return "no_character";

		LF_AssetAnimComponent anim = LF_AssetAnimComponent.Nearest(character.GetOrigin(), 25);
		if (!anim)
			return "asset=none";

		IEntity owner = anim.GetOwner();
		string detail = string.Format("asset=%1 running=%2 shown=%3 posed=%4 effects=%5 sounds=%6", FilePath.StripPath(LF_Utils.GetPrefabName(owner)), anim.IsRunning(), anim.IsShownActive(), anim.IsPosed(), anim.GetLiveEffectCount(), anim.GetLiveSoundCount());
		detail += string.Format(" height=%1 distance=%2", owner.GetOrigin()[1] - character.GetOrigin()[1], vector.Distance(owner.GetOrigin(), character.GetOrigin()));
		Animation animation = owner.GetAnimation();
		string bone = anim.GetBoneName(0);
		if (animation && !bone.IsEmpty())
		{
			vector mat[4];
			// Model space: the bone's own x axis, which sweeps round as the part turns.
			animation.GetBoneMatrix(animation.GetBoneIndex(bone), mat);
			detail += string.Format(" %1_x=%2,%3,%4", bone, mat[0][0], mat[0][1], mat[0][2]);
		}

		return detail;
	}
}
