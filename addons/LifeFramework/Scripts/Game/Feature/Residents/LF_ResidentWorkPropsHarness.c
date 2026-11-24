// Harness verb `workprop <carry|sweep> <ox,oy,oz> <ux,uy,uz> [bone]` (#4644), for fitting the box and the broom
// to the hands. It runs on the client the verb is sent to and moves the locally spawned work props of the
// characters within 40 m of the player: the prop's origin goes to (ox,oy,oz) and its up axis along (ux,uy,uz),
// both in the CHARACTER's space (metres, x right, y up, z forward: the same space as the hand targets in
// tools/assets/work-anim/clips), its forward axis as near the character's forward as the up axis allows. An
// optional bone name hangs the prop from that bone instead. The answer carries the bone-space position and
// angles that give the same placement, which are the numbers LF_ResidentWorkProps.Item takes. Local to the
// client, no state, nothing replicated. `ok moved=<n> pos=<x,y,z> ang=<a,b,c> bone=<name>` or an error.

//------------------------------------------------------------------------------------------------
//! Collects the characters near a point.
class LF_WorkPropHarnessQuery
{
	ref array<IEntity> m_aCharacters = {};

	//------------------------------------------------------------------------------------------------
	bool Collect(IEntity entity)
	{
		if (entity && SCR_ChimeraCharacter.Cast(entity))
			m_aCharacters.Insert(entity);

		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_ResidentWorkPropsHarness
{
	//------------------------------------------------------------------------------------------------
	protected static vector Triple(string text)
	{
		array<string> parts = {};
		text.Split(",", parts, true);
		if (parts.Count() != 3)
			return vector.Zero;

		return Vector(parts[0].ToFloat(), parts[1].ToFloat(), parts[2].ToFloat());
	}

	//------------------------------------------------------------------------------------------------
	//! The matrix with `up` as its Y axis and its Z axis as near `forward` as that allows.
	protected static void Basis(vector up, out vector target[4])
	{
		vector y = up.Normalized();
		vector forward = "0 0 1";
		vector z = forward - y * vector.Dot(forward, y);
		if (z.Length() < 0.001)
			z = "1 0 0";

		z = z.Normalized();
		vector x = Vector(y[1] * z[2] - y[2] * z[1], y[2] * z[0] - y[0] * z[2], y[0] * z[1] - y[1] * z[0]);
		target[0] = x;
		target[1] = y;
		target[2] = z;
	}

	//------------------------------------------------------------------------------------------------
	//! \return "ok ..." or "error <why>".
	static string Fit(LF_AgentCommand cmd)
	{
		ResourceName prefab = LF_ResidentWorkProps.BOX_PREFAB;
		if (cmd.Arg1 == LF_ResidentWorkLoop.SWEEP)
			prefab = LF_ResidentWorkProps.BROOM_PREFAB;
		else if (cmd.Arg1 != LF_ResidentWorkLoop.CARRY)
			return "error job is carry or sweep";

		string boneName = LF_ResidentWorkProps.HAND_BONE;
		if (!cmd.Arg4.IsEmpty())
			boneName = cmd.Arg4;

		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (!player)
			return "error no local character";

		LF_WorkPropHarnessQuery query = new LF_WorkPropHarnessQuery();
		GetGame().GetWorld().QueryEntitiesBySphere(player.GetOrigin(), 40, query.Collect);
		vector target[4];
		Basis(Triple(cmd.Arg3), target);
		target[3] = Triple(cmd.Arg2);
		int moved;
		string report;
		foreach (IEntity character : query.m_aCharacters)
		{
			if (character == player)
				continue;

			Animation animation = character.GetAnimation();
			if (!animation)
				continue;

			TNodeId bone = animation.GetBoneIndex(boneName);
			vector boneModel[4];
			if (bone < 0 || !animation.GetBoneMatrix(bone, boneModel))
				return "error no bone " + boneName;

			vector local[4];
			Math3D.MatrixInvMultiply4(boneModel, target, local);
			IEntity child = character.GetChildren();
			while (child)
			{
				IEntity next = child.GetSibling();
				EntityPrefabData data = child.GetPrefabData();
				if (data && LF_Utils.SamePrefab(data.GetPrefabName(), prefab))
				{
					character.RemoveChild(child);
					character.AddChild(child, bone);
					child.SetLocalTransform(local);
					moved++;
					vector angles = Math3D.MatrixToAngles(local);
					report = string.Format("pos=%1,%2,%3 ang=%4,%5,%6 bone=%7", local[3][0], local[3][1], local[3][2], angles[0], angles[1], angles[2], boneName);
				}

				child = next;
			}
		}

		return string.Format("ok moved=%1 %2", moved, report);
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "workprop")
			return super.Handle(cmd);

		string result = LF_ResidentWorkPropsHarness.Fit(cmd);
		if (result.IndexOf("ok") == 0)
			LF_AgentHarness.Answer(cmd.Id, "ok", result.Substring(3, result.Length() - 3));
		else
			LF_AgentHarness.Answer(cmd.Id, "error", result.Substring(6, result.Length() - 6));

		return true;
	}
}
