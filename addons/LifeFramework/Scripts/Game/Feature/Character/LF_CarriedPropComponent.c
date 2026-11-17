//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Character", description: "Keeps a prop in a hand slot hanging upright from its handle")]
class LF_CarriedPropComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! A prop carried by a handle (#2174: the relocated walkers' suitcases). The prop sits in a
//! SlotManagerComponent slot on the hand bone; a slot transform is fixed to the bone, so the case
//! turned with the hand through every arm swing. Each frame this puts the handle point on the hand
//! bone and the prop upright, its long side (model X) along the character's flat forward axis: it
//! hangs from the hand. The base game has no grip pose for such a prop, so the fingers stay as the
//! walk animation leaves them. Visual only: not run on a dedicated server, where the slot's own
//! transform (the same pose at rest) stands.
class LF_CarriedPropComponent : ScriptComponent
{
	static const string FEATURE = "Character";

	[Attribute("Suitcase", UIWidgets.EditBox, "SlotManagerComponent slot that holds the prop")]
	protected string m_sSlot;

	[Attribute("RightHandProp", UIWidgets.EditBox, "Bone the slot hangs from")]
	protected string m_sBone;

	[Attribute("0 0.455 0.02", UIWidgets.EditBox, "The handle's grip point in the prop model")]
	protected vector m_vHandle;

	//! Frames in a row without the prop or the bone before the component gives up (about 10 s).
	static const int MISSING_FRAMES = 300;
	protected int m_iMissing;

	//! Beyond this distance from the local camera the prop is not hung (the case is a few pixels there).
	static const float VIEW_DISTANCE = 60;

	//! Slot and bone index found on the first good frame, reused after that.
	protected EntitySlotInfo m_Slot;
	protected TNodeId m_Bone = -1;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		// Play mode only (as LF_FittedBuildingComponent): in the World Editor the repeating call would
		// override the slot transform on placed walkers.
		if (RplSession.Mode() == RplMode.Dedicated || !GetGame() || !GetGame().InPlayMode() || !GetGame().GetCallqueue())
			return;

		// Every frame from the call queue: a character proxy does not get EOnFrame from the event mask
		// alone (#2174 grip: OnPostInit ran on the client, EOnFrame never did).
		GetGame().GetCallqueue().CallLater(Hang, 0, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Hang);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! False when this frame's hang can be skipped: the walker was not rendered last frame, or is
	//! farther than VIEW_DISTANCE from the camera (`distanceSq` is the squared distance).
	static bool ShouldHang(bool rendered, float distanceSq)
	{
		if (!rendered)
			return false;

		return distanceSq <= VIEW_DISTANCE * VIEW_DISTANCE;
	}

	//------------------------------------------------------------------------------------------------
	//! The missing-frames-in-a-row counter after one frame: a good frame resets it.
	static int NextMissing(bool hung, int missing)
	{
		if (hung)
			return 0;

		return missing + 1;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the counter has reached the give-up point.
	static bool GivesUp(int missing)
	{
		return missing >= MISSING_FRAMES;
	}

	//------------------------------------------------------------------------------------------------
	//! Hangs the prop from the hand for this frame.
	protected void Hang()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		// A walker out of sight or far away is not worth a frame; skipped frames are not missing frames.
		if (!ShouldHang(owner.IsRendered(), DistanceSqToCamera(owner)))
			return;

		if (!m_Slot || !m_Slot.GetAttachedEntity())
		{
			m_Slot = null;
			SlotManagerComponent slots = SlotManagerComponent.Cast(owner.FindComponent(SlotManagerComponent));
			if (slots)
				m_Slot = slots.GetSlotByName(m_sSlot);
		}

		vector pivot[4];
		bool hung = m_Slot && m_Slot.GetAttachedEntity() && BonePivot(owner, pivot);
		m_iMissing = NextMissing(hung, m_iMissing);
		if (!hung)
		{
			// The prop may attach a few frames after init; give up (logged) after MISSING_FRAMES in a row.
			if (!GivesUp(m_iMissing))
				return;

			LF_Debug.Warn(FEATURE, string.Format("%1: no prop in slot '%2' or no bone '%3'; the carried prop is not held upright", LF_Utils.GetPrefabName(owner), m_sSlot, m_sBone));
			GetGame().GetCallqueue().Remove(Hang);
			return;
		}

		vector body[4];
		owner.GetWorldTransform(body);
		vector local[4];
		SolveHanging(pivot, body[2], m_vHandle, local);
		m_Slot.OverrideTransformLS(local);
		// As vanilla does after a slot transform change (AttachToSlotManagerCinematicTrack): without it the
		// prop kept a stale hierarchy transform, gone from the hand or a metre or two off it (#2174 grip).
		m_Slot.GetAttachedEntity().Update();
	}

	//------------------------------------------------------------------------------------------------
	//! Squared distance from the local camera to `owner`; 0 when there is no camera (hang anyway).
	protected static float DistanceSqToCamera(notnull IEntity owner)
	{
		CameraManager cameras = GetGame().GetCameraManager();
		if (!cameras)
			return 0;

		CameraBase camera = cameras.CurrentCamera();
		if (!camera)
			return 0;

		vector view[4];
		camera.GetWorldCameraTransform(view);
		vector d = owner.GetOrigin() - view[3];
		return d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
	}

	//------------------------------------------------------------------------------------------------
	//! The hand bone's world transform, with the bone index cached after the first success.
	protected bool BonePivot(notnull IEntity person, out vector pivot[4])
	{
		if (m_Bone < 0)
		{
			Animation anim = person.GetAnimation();
			if (!anim)
				return false;

			m_Bone = anim.GetBoneIndex(m_sBone);
			if (m_Bone < 0)
				return false;
		}

		Animation animation = person.GetAnimation();
		vector boneModel[4];
		if (!animation || !animation.GetBoneMatrix(m_Bone, boneModel))
			return false;

		vector body[4];
		person.GetWorldTransform(body);
		Math3D.MatrixMultiply4(body, boneModel, pivot);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The bone's world transform. False when the character has no such bone.
	static bool HandPivot(notnull IEntity person, string boneName, out vector pivot[4])
	{
		Animation anim = person.GetAnimation();
		if (!anim)
			return false;

		TNodeId bone = anim.GetBoneIndex(boneName);
		vector boneModel[4];
		if (bone < 0 || !anim.GetBoneMatrix(bone, boneModel))
			return false;

		vector body[4];
		person.GetWorldTransform(body);
		Math3D.MatrixMultiply4(body, boneModel, pivot);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The slot-local transform that hangs the prop from a hand at `pivot`: upright, long side along
	//! `forward` flattened, model point `handle` on the pivot.
	static void SolveHanging(vector pivot[4], vector forward, vector handle, out vector local[4])
	{
		vector x = Vector(forward[0], 0, forward[2]).Normalized();
		vector y = "0 1 0";
		vector z = Vector(x[1] * y[2] - x[2] * y[1], x[2] * y[0] - x[0] * y[2], x[0] * y[1] - x[1] * y[0]);
		vector world[4];
		world[0] = x;
		world[1] = y;
		world[2] = z;
		world[3] = pivot[3] - x * handle[0] - y * handle[1] - z * handle[2];
		Math3D.MatrixInvMultiply4(pivot, world, local);
	}
}
