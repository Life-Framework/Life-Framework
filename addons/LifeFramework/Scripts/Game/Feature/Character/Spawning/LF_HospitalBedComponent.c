// LF_HospitalBedComponent.c — marks a bed a dead player can wake in. Put it on any bed prop in
// the world; the set of placed beds is the hospital respawn point list (LF_RespawnConfig turns
// the hospital on or off). Beds register by EntityID and are re-resolved on every use.

[ComponentEditorProps(category: "LifeFramework/Respawn", description: "A hospital bed a dead player wakes in")]
class LF_HospitalBedComponentClass : ScriptComponentClass
{
}

class LF_HospitalBedComponent : ScriptComponent
{
	[Attribute("0 0 0", UIWidgets.EditBox, "Where the waking body stands, relative to the bed origin (bed space, metres)")]
	protected vector m_vWakeOffset;

	[Attribute("0", UIWidgets.EditBox, "Yaw of the waking body relative to the bed, degrees")]
	protected float m_fWakeYaw;

	[Attribute("0.85 0 0", UIWidgets.EditBox, "Where the woken body steps down to beside the bed, relative to the bed origin (bed space, metres)")]
	protected vector m_vStandOffset;

	protected static ref array<EntityID> s_aBeds;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!s_aBeds)
			s_aBeds = {};

		if (s_aBeds.Find(owner.GetID()) < 0)
			s_aBeds.Insert(owner.GetID());
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_aBeds)
			s_aBeds.RemoveItem(owner.GetID());

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! World transform of a patient laid in the bed: on the mattress top (LF_BedWakePose.LayPoint, from
	//! the bed's real bounds) plus the configured offset, facing the bed's yaw plus the configured yaw.
	//! The bed origin is on the floor under the frame, so it is not the lay point (#5153).
	void GetWakeTransform(out vector transform[4])
	{
		IEntity owner = GetOwner();
		vector bed[4];
		owner.GetWorldTransform(bed);

		vector angles = owner.GetYawPitchRoll();
		Math3D.AnglesToMatrix(Vector(angles[0] + m_fWakeYaw, 0, 0), transform);
		transform[3] = LF_BedWakePose.LayPoint(owner) + m_vWakeOffset.Multiply3(bed);
	}

	//------------------------------------------------------------------------------------------------
	//! World transform a respawned body wakes at: feet on the floor at the free side, back to the
	//! mattress, where the bed wake animation sits up from (LF_BedWakePose). The wake transform
	//! above is the mattress, for a patient laid in the bed.
	void GetSitTransform(out vector transform[4])
	{
		LF_BedWakePose.SitTransform(GetOwner(), m_vStandOffset, transform);
	}

	//------------------------------------------------------------------------------------------------
	//! World position on the floor beside the bed where the woken body ends up (probed floor height).
	//! \param ignore A body to see through (the owner's own, standing at the bed).
	vector GetStandPosition(IEntity ignore = null)
	{
		return LF_BedWakePose.StandOnFloor(GetOwner(), m_vStandOffset, ignore);
	}

	//------------------------------------------------------------------------------------------------
	//! The floor spot a patient laid in the bed steps down to, when one has room (both sides, the
	//! foot, wider rings). False: no spot, the patient stays on the mattress.
	bool FindStandSpot(IEntity ignore, out vector spot)
	{
		return LF_BedWakePose.FindStandSpot(GetOwner(), m_vStandOffset, ignore, spot);
	}

	//------------------------------------------------------------------------------------------------
	//! The bed whose wake point is nearest a position (horizontal), within maxDistance metres.
	static LF_HospitalBedComponent FindNearest(vector position, float maxDistance)
	{
		array<LF_HospitalBedComponent> beds = {};
		GetBeds(beds);
		LF_HospitalBedComponent nearest;
		float best = maxDistance * maxDistance;
		foreach (LF_HospitalBedComponent bed : beds)
		{
			vector transform[4];
			bed.GetWakeTransform(transform);
			float distance = vector.DistanceSqXZ(transform[3], position);
			if (distance <= best)
			{
				best = distance;
				nearest = bed;
			}
		}

		return nearest;
	}

	//------------------------------------------------------------------------------------------------
	//! Every live bed in the world, in registration order. Stale ids are skipped.
	static void GetBeds(notnull array<LF_HospitalBedComponent> outBeds)
	{
		outBeds.Clear();
		if (!s_aBeds)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		foreach (EntityID id : s_aBeds)
		{
			IEntity entity = world.FindEntityByID(id);
			if (!entity)
				continue;

			LF_HospitalBedComponent bed = LF_HospitalBedComponent.Cast(entity.FindComponent(LF_HospitalBedComponent));
			if (bed)
				outBeds.Insert(bed);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The wake position of every live bed, in the same order as GetBeds.
	static void GetBedPositions(notnull array<vector> outPositions)
	{
		outPositions.Clear();
		array<LF_HospitalBedComponent> beds = {};
		GetBeds(beds);
		foreach (LF_HospitalBedComponent bed : beds)
		{
			vector transform[4];
			bed.GetWakeTransform(transform);
			outPositions.Insert(transform[3]);
		}
	}
}
