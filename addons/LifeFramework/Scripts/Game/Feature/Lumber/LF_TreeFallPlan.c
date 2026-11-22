//------------------------------------------------------------------------------------------------
//! Where a felled tree's pieces land (design docs/features/gathering-processing-resources.md "Tree fall").
//! Pure: no world, no entities. The tree falls away from the player who cut it; its trunk becomes the logs,
//! laid end to end along the fallen line from the stump outward, and the crown becomes a few branch props
//! further along. LF_TreeFall spawns what this plans and probes each spot down to the ground.

//------------------------------------------------------------------------------------------------
//! One planned piece: where its centre lies (y is the foot's height, the spawn probes the ground) and its yaw.
class LF_TreeFallSpot : Managed
{
	vector m_vPos;
	float m_fYawDeg;

	//------------------------------------------------------------------------------------------------
	static LF_TreeFallSpot Make(vector pos, float yawDeg)
	{
		LF_TreeFallSpot spot = new LF_TreeFallSpot();
		spot.m_vPos = pos;
		spot.m_fYawDeg = yawDeg;
		return spot;
	}
}

//------------------------------------------------------------------------------------------------
class LF_TreeFallPlan
{
	//! Metres between the stump and the first log.
	static const float LOG_START_M = 0.6;
	//! Metres between the end of one log and the start of the next.
	static const float LOG_GAP_M = 0.3;
	//! Sideways step when the trunk is too short for the logs end to end: neighbours lie side by side.
	static const float LOG_SIDE_M = 0.45;
	//! The part of the tree height the trunk logs may cover; the crown above it is branches.
	static const float TRUNK_SHARE = 0.7;
	//! Shortest and tallest height the plan believes (a broken bounds read stays sane).
	static const float MIN_HEIGHT_M = 5;
	static const float MAX_HEIGHT_M = 32;
	//! Branch props per tree, never more than this.
	static const int MAX_DEBRIS = 5;	// lf-fixed: a perf cap on props per tree

	//! The fall, in milliseconds: the trunk tips over, lands, bounces once and is replaced by the logs.
	static const int FALL_MS = 2800;	// lf-fixed: animation timing; the logs drop when it ends
	static const int BOUNCE_MS = 350;	// lf-fixed: animation timing
	//! How far over the trunk lies when it lands (the crown holds it a little off flat).
	static const float LAND_DEG = 86;
	static const float BOUNCE_DEG = 5;

	//------------------------------------------------------------------------------------------------
	//! The yaw (degrees, Enfusion: 0 is +z, 90 is +x) that points from the player through the tree, so it
	//! falls away from them. A player standing on the trunk's own spot gets 0.
	static float FallYaw(vector user, vector tree)
	{
		float dx = tree[0] - user[0];
		float dz = tree[2] - user[2];
		if (Math.AbsFloat(dx) < 0.001 && Math.AbsFloat(dz) < 0.001)
			return 0;

		return Math.Atan2(dx, dz) * Math.RAD2DEG;
	}

	//------------------------------------------------------------------------------------------------
	//! The flat unit direction of a yaw.
	static vector Dir(float yawDeg)
	{
		float rad = yawDeg * Math.DEG2RAD;
		return Vector(Math.Sin(rad), 0, Math.Cos(rad));
	}

	//------------------------------------------------------------------------------------------------
	static float ClampHeight(float heightM)
	{
		return Math.Clamp(heightM, MIN_HEIGHT_M, MAX_HEIGHT_M);
	}

	//------------------------------------------------------------------------------------------------
	//! One spot per log, `count` of them: centres along the fallen line, each log turned along it. When the
	//! trunk is too short for the logs end to end they close up and step sideways in turn, so no two overlap.
	//! \param logLenM the log mesh's length (LF_TreeFall measures it)
	static array<ref LF_TreeFallSpot> LogSpots(vector foot, float yawDeg, float heightM, float logLenM, int count)
	{
		array<ref LF_TreeFallSpot> spots = {};
		if (count < 1)
			return spots;

		if (logLenM < 0.5)
			logLenM = 0.5;

		vector dir = Dir(yawDeg);
		vector side = Vector(dir[2], 0, -dir[0]);
		float trunk = ClampHeight(heightM) * TRUNK_SHARE - LOG_START_M;
		float pitch = logLenM + LOG_GAP_M;
		bool stagger = false;
		if (count * pitch > trunk && count > 1)
		{
			pitch = Math.Max(trunk - logLenM, 0) / (count - 1);
			stagger = pitch < logLenM + LOG_GAP_M;
			// Every other log shares a side, so two on one side stay a whole log apart; the row runs past a
			// short trunk rather than stacking logs inside each other.
			if (stagger)
				pitch = Math.Max(pitch, (logLenM + LOG_GAP_M) * 0.5);
		}

		for (int i = 0; i < count; i++)
		{
			float along = LOG_START_M + logLenM / 2 + i * pitch;
			float across = 0;
			if (stagger)
				across = ((i % 2) * 2 - 1) * LOG_SIDE_M;

			// A small turn per log so the row does not look stamped.
			float yaw = yawDeg + ((i % 3) - 1) * 4;
			spots.Insert(LF_TreeFallSpot.Make(foot + dir * along + side * across, yaw));
		}

		return spots;
	}

	//------------------------------------------------------------------------------------------------
	//! How many branch props a tree of this size leaves: 2 for a small one, one more per size, at most MAX_DEBRIS.
	static int DebrisCount(int size)
	{
		return LF_Utils.ClampInt(size + 2, 2, MAX_DEBRIS);
	}

	//------------------------------------------------------------------------------------------------
	//! Branch props where the crown landed: past the trunk logs, spread a little to each side. The same
	//! seed gives the same spots on every machine.
	static array<ref LF_TreeFallSpot> DebrisSpots(vector foot, float yawDeg, float heightM, int count, int seed)
	{
		array<ref LF_TreeFallSpot> spots = {};
		RandomGenerator random = new RandomGenerator();
		random.SetSeed(seed);
		vector dir = Dir(yawDeg);
		vector side = Vector(dir[2], 0, -dir[0]);
		float height = ClampHeight(heightM);
		for (int i = 0; i < count; i++)
		{
			float along = random.RandFloatXY(height * 0.45, height * 0.95);
			float across = random.RandFloatXY(-2.2, 2.2);
			float yaw = random.RandFloatXY(0, 360);
			spots.Insert(LF_TreeFallSpot.Make(foot + dir * along + side * across, yaw));
		}

		return spots;
	}

	//------------------------------------------------------------------------------------------------
	//! How far over the falling trunk is, `elapsedMs` after the fell: slow at first, fast at the end (it
	//! falls), a small bounce on landing, then it lies at LAND_DEG.
	static float AngleAt(int elapsedMs)
	{
		if (elapsedMs <= 0)
			return 0;

		if (elapsedMs <= FALL_MS)
		{
			float u = elapsedMs / (FALL_MS * 1.0);
			return LAND_DEG * u * u;
		}

		if (elapsedMs <= FALL_MS + BOUNCE_MS)
		{
			float b = (elapsedMs - FALL_MS) / (BOUNCE_MS * 1.0);
			return LAND_DEG - BOUNCE_DEG * Math.Sin(b * Math.PI);
		}

		return LAND_DEG;
	}

	//------------------------------------------------------------------------------------------------
	//! `v` turned by `deg` about the unit `axis` (Rodrigues). Tipping a tree toward `dir` turns its up axis
	//! about up x dir.
	static vector Rotate(vector v, vector axis, float deg)
	{
		float rad = deg * Math.DEG2RAD;
		float c = Math.Cos(rad);
		float s = Math.Sin(rad);
		return v * c + Cross(axis, v) * s + axis * (vector.Dot(axis, v) * (1 - c));
	}

	//------------------------------------------------------------------------------------------------
	static vector Cross(vector a, vector b)
	{
		return Vector(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]);
	}
}

//------------------------------------------------------------------------------------------------
//! What the loose-log sweep does with one log it dropped.
enum LF_LooseLogFate
{
	KEEP,
	//! Picked up, carried, loaded or moved by a player: no longer the sweep's to clean.
	FORGET,
	//! Lay where it fell past its time: the sweep deletes it.
	DELETE
}

//------------------------------------------------------------------------------------------------
//! A log a fell dropped and nobody has taken yet.
class LF_LooseLog : Managed
{
	EntityID m_Id;
	//! Where it lies: the spawn spot, then where it came to rest (the first sweep after the drop).
	vector m_vAt;
	bool m_bSettled;
	int m_iBornS;
}

//------------------------------------------------------------------------------------------------
//! The server's book of logs lying where a tree fell (pure: the caller resolves entities). A log a player
//! takes leaves the book and is never deleted by the sweep; one left lying past its time is deleted, and
//! past the cap the oldest lying ones go first, so felling all day does not pile props up.
class LF_LooseLogBook : Managed
{
	//! Metres a log may roll while it settles and still count as lying where it fell.
	static const float MOVED_M = 6;

	ref array<ref LF_LooseLog> m_aLogs = {};

	//------------------------------------------------------------------------------------------------
	void Add(EntityID id, vector at, int nowS)
	{
		LF_LooseLog log = new LF_LooseLog();
		log.m_Id = id;
		log.m_vAt = at;
		log.m_iBornS = nowS;
		m_aLogs.Insert(log);
	}

	//------------------------------------------------------------------------------------------------
	int Count()
	{
		return m_aLogs.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! \param exists the entity still resolves
	//! \param taken it sits in a slot or under a parent (a shoulder, a hand, a bed, a container)
	//! \param nowAt where it is now
	static LF_LooseLogFate Fate(notnull LF_LooseLog log, bool exists, bool taken, vector nowAt, int nowS, int lifeS)
	{
		if (!exists || taken)
			return LF_LooseLogFate.FORGET;

		if (vector.DistanceXZ(log.m_vAt, nowAt) > MOVED_M)
			return LF_LooseLogFate.FORGET;

		if (nowS - log.m_iBornS >= lifeS)
			return LF_LooseLogFate.DELETE;

		return LF_LooseLogFate.KEEP;
	}

	//------------------------------------------------------------------------------------------------
	//! How many of the oldest logs to delete so the book holds at most `cap`.
	int OverCap(int cap)
	{
		return LF_Utils.MaxInt(m_aLogs.Count() - cap, 0);
	}
}
