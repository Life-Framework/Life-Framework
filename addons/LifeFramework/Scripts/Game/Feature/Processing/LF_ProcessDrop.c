//------------------------------------------------------------------------------------------------
//! Where a machine puts the goods it drops (the sawmill's boards, the smelter's bars): on the side of
//! the machine the operator stands, as far out as the prefab's drop offset reaches. The offset used to be
//! added in world axes, whatever way the machine was turned, and always on one side: the boards landed
//! 2.8 m from a player 1 m east of the woodlot saw and 4.3 m from one 2.5 m east, past the 3 m pickup
//! reach (11-27 dry run 3). The offset is a machine-local point now (rotated by the machine's transform)
//! and the drop swings to the worker's side of the machine.
class LF_ProcessDrop
{
	//! A user closer to the machine than this (horizontally) has no side; the machine-local offset is used.
	// lf-fixed: geometry guard against a zero-length direction, not a gameplay tunable
	static const float NO_SIDE_M = 0.05;

	//------------------------------------------------------------------------------------------------
	//! \return The drop point for a machine at \p machineOrigin worked by a user at \p userOrigin: the
	//! horizontal length of \p offset from the machine toward the user, at the offset's height. \p fallback
	//! (the offset in the machine's own frame, in world coordinates) when the offset has no horizontal
	//! length or the user stands on the machine.
	static vector Point(vector machineOrigin, vector userOrigin, vector offset, vector fallback)
	{
		float reach = Math.Sqrt(offset[0] * offset[0] + offset[2] * offset[2]);
		if (reach < NO_SIDE_M)
			return fallback;

		float dx = userOrigin[0] - machineOrigin[0];
		float dz = userOrigin[2] - machineOrigin[2];
		float length = Math.Sqrt(dx * dx + dz * dz);
		if (length < NO_SIDE_M)
			return fallback;

		vector point = machineOrigin;
		point[0] = machineOrigin[0] + dx / length * reach;
		point[1] = machineOrigin[1] + offset[1];
		point[2] = machineOrigin[2] + dz / length * reach;
		return point;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The drop point of \p machine for \p user: the operator-side point, or the machine-local
	//! \p offset turned by the machine's transform when there is no user.
	static vector PointFor(IEntity machine, IEntity user, vector offset)
	{
		if (!machine)
			return offset;

		vector local = machine.CoordToParent(offset);
		if (!user)
			return local;

		return Point(machine.GetOrigin(), user.GetOrigin(), offset, local);
	}
}
