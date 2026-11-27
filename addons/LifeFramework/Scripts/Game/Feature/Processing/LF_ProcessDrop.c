//------------------------------------------------------------------------------------------------
//! Where a machine puts the goods it drops (the sawmill's boards, the smelter's bars): on the side of
//! the machine the operator stands, as far out as the prefab's drop offset reaches. A fixed world-axis
//! offset landed the boards on one side of the saw whichever side the player worked it from, 2.8 m from
//! a player 1 m east of it and 4.3 m from one 2.5 m east, past the 3 m pickup reach (11-27 dry run 3).
class LF_ProcessDrop
{
	//! A user closer to the machine than this (horizontally) has no side; the prefab's own offset is used.
	// lf-fixed: geometry guard against a zero-length direction, not a gameplay tunable
	static const float NO_SIDE_M = 0.05;

	//------------------------------------------------------------------------------------------------
	//! \return The drop point for a machine at \p machineOrigin worked by a user at \p userOrigin:
	//! the horizontal length of \p offset from the machine toward the user, at the offset's height.
	//! The prefab's own offset when there is no user, no horizontal offset, or the user stands on the machine.
	static vector Point(vector machineOrigin, vector userOrigin, vector offset)
	{
		float reach = Math.Sqrt(offset[0] * offset[0] + offset[2] * offset[2]);
		if (reach < NO_SIDE_M)
			return machineOrigin + offset;

		float dx = userOrigin[0] - machineOrigin[0];
		float dz = userOrigin[2] - machineOrigin[2];
		float length = Math.Sqrt(dx * dx + dz * dz);
		if (length < NO_SIDE_M)
			return machineOrigin + offset;

		vector point = machineOrigin;
		point[0] = machineOrigin[0] + dx / length * reach;
		point[1] = machineOrigin[1] + offset[1];
		point[2] = machineOrigin[2] + dz / length * reach;
		return point;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The drop point of \p machine for \p user (null user: the prefab's offset).
	static vector PointFor(IEntity machine, IEntity user, vector offset)
	{
		if (!machine)
			return offset;

		if (!user)
			return machine.GetOrigin() + offset;

		return Point(machine.GetOrigin(), user.GetOrigin(), offset);
	}
}
