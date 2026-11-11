//! One entity's placement measurements (world metres, degrees). LF_PlacementAudit.Measure fills it.
class LF_PlacementMeasure
{
	vector m_vMins;
	vector m_vMaxs;
	//! World bounds bottom minus the surface under the centre (+ floats, - sunk).
	float m_fGap;
	//! Pivot height above that surface.
	float m_fPivotGap;
	float m_fTilt;
	float m_fScale = 1;
	//! What the centre rests on: an entity label, or "terrain".
	string m_sSupport = "terrain";
	//! Bottom corners (of 4) with no surface within OVERHANG_DROP below them, and the largest drop.
	int m_iOverhang;
	float m_fOverDrop;
	//! A wall or other entity within 0.3 m of a side: a hung sign or board is mounted, not floating.
	bool m_bBacked;
	//! No mesh bounds (an action anchor, a spawn point, a manager): nothing to see, so nothing to flag.
	bool m_bPoint;
	//! A character (an NPC): its box is a standing body at the origin, not the unanimated mesh bounds.
	bool m_bCharacter;
	string m_sClips = "none";
	//! Edge to the nearest standing spot (99 = none within 3 m); -1 when not measured.
	float m_fWalk = -1;
	//! Oriented box: centre, half extents along each unit axis (scale applied).
	vector m_vCenter;
	vector m_vHalf;
	vector m_vAxis0;
	vector m_vAxis1;
	vector m_vAxis2;
}

//! Read-only placement checks for the agent harness (`probe ent`, `probe batch`) and its tests:
//! the floor under an entity (floating / sunk), corners hanging off a surface edge, clips through
//! other surfaces, tilt, scale, reach, and the oriented box the Node side uses for overlaps.
//! Traces only; it never moves anything. Works on a client and on a dedicated server.
class LF_PlacementAudit
{
	static const float GAP = 0.15;
	static const float TILT = 5;
	static const float SCALE_MIN = 0.5;
	static const float SCALE_MAX = 2.0;
	static const float OVERHANG_DROP = 0.2;
	static const float OVERHANG_INSET = 0.2;

	protected static bool s_bAllLayers;
	protected static string s_sClipNames;
	protected static int s_iClipCount;

	//------------------------------------------------------------------------------------------------
	//! Distance along from->to to the first hit (the full length when clear).
	static float Trace(vector from, vector to, IEntity exclude, out IEntity hitEntity, bool allLayers = false)
	{
		TraceParam trace = new TraceParam();
		if (allLayers || s_bAllLayers)
			trace.TargetLayers = 0xFFFFFFFF;
		trace.Start = from;
		trace.End = to;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.Exclude = exclude;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		float fraction = GetGame().GetWorld().TraceMove(trace, null);
		hitEntity = trace.TraceEnt;
		return fraction * vector.Distance(from, to);
	}

	//------------------------------------------------------------------------------------------------
	//! Like Trace, against every physics layer: what a thing rests on or leans against. Small props
	//! such as BoxWooden_01 have no projectile geometry above their base plate, so the projectile
	//! trace went straight through a crate and called the one stacked on it floating.
	static float SurfaceTrace(vector from, vector to, IEntity exclude, out IEntity hitEntity)
	{
		TraceParam trace = new TraceParam();
		trace.Start = from;
		trace.End = to;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.Exclude = exclude;
		trace.LayerMask = -1;
		float fraction = GetGame().GetWorld().TraceMove(trace, null);
		hitEntity = trace.TraceEnt;
		return fraction * vector.Distance(from, to);
	}

	//------------------------------------------------------------------------------------------------
	//! True when `other` is `self` or one of its descendants (a parent or sibling is something else).
	static bool IsOwnPart(IEntity self, IEntity other)
	{
		IEntity walk = other;
		while (walk)
		{
			if (walk == self)
				return true;

			walk = walk.GetParent();
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Name of an entity for a report: its world name, else its prefab file name.
	static string Label(IEntity other)
	{
		if (!other)
			return "none";

		string label = other.GetName();
		if (label.IsEmpty())
		{
			EntityPrefabData prefab = other.GetPrefabData();
			if (prefab)
				label = FilePath.StripPath(prefab.GetPrefabName());
		}

		if (label.IsEmpty())
			label = other.ClassName();

		label.Replace(".et", "");
		label.Replace(" ", "");
		label.Replace(",", "");
		label.Replace("=", "");
		label.Replace(";", "");
		if (label.IsEmpty())
			label = "unnamed";

		return label;
	}

	//------------------------------------------------------------------------------------------------
	//! Prefab file name without folder or extension, or "".
	static string PrefabBase(IEntity entity)
	{
		if (!entity)
			return "";

		EntityPrefabData prefab = entity.GetPrefabData();
		if (!prefab)
			return "";

		string base = FilePath.StripPath(prefab.GetPrefabName());
		base.Replace(".et", "");
		return base;
	}

	//------------------------------------------------------------------------------------------------
	//! The surface under (x, z): the terrain, or the highest surface between startY and 5 m below it
	//! that is not part of `self`. `supportLabel` names it.
	static float SupportAt(IEntity self, float x, float z, float startY, out string supportLabel)
	{
		BaseWorld world = GetGame().GetWorld();
		float support = world.GetSurfaceY(x, z);
		supportLabel = "terrain";
		float fromY = startY;
		// Something whose bounds start above this entity's bottom rests on it (a crate stacked on a
		// crate, a vice on a bench): it is not the floor, so the trace goes on below it.
		float bottom = startY - 0.5;
		if (self)
		{
			vector smin;
			vector smax;
			self.GetWorldBounds(smin, smax);
			bottom = smin[1];
		}
		for (int n = 0; n < 6; n++)
		{
			IEntity hit;
			float down = SurfaceTrace(Vector(x, fromY, z), Vector(x, fromY - 5, z), self, hit);
			if (down >= 5)
				break;

			float hitY = fromY - down;
			bool above = false;
			if (hit)
			{
				vector hmin;
				vector hmax;
				hit.GetWorldBounds(hmin, hmax);
				above = hmin[1] > bottom + 0.1;
			}

			if (hit && (IsOwnPart(self, hit) || ChimeraCharacter.Cast(hit) || above))
			{
				fromY = hitY - 0.02;
				continue;
			}

			if (hitY > support)
			{
				support = hitY;
				if (hit)
					supportLabel = Label(hit);
				else
					supportLabel = "ground";
			}

			break;
		}

		return support;
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity s_ColumnSelf;
	protected static vector s_vColumn;
	protected static float s_fColumnTop;
	protected static IEntity s_ColumnBest;

	protected static bool CollectColumn(IEntity other)
	{
		if (!other || IsOwnPart(s_ColumnSelf, other) || ChimeraCharacter.Cast(other))
			return true;

		typename type = other.Type();
		if (type.IsInherited(GenericTerrainEntity) || type.IsInherited(GenericWorldEntity))
			return true;

		vector omin;
		vector omax;
		other.GetWorldBounds(omin, omax);
		if ((omax[0] - omin[0]) * (omax[1] - omin[1]) * (omax[2] - omin[2]) > 40)
			return true;
		if (s_vColumn[0] < omin[0] || s_vColumn[0] > omax[0] || s_vColumn[2] < omin[2] || s_vColumn[2] > omax[2])
			return true;
		if (omax[1] > s_vColumn[1] + 0.05 || omax[1] <= s_fColumnTop)
			return true;

		s_fColumnTop = omax[1];
		s_ColumnBest = other;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The top of the highest prop whose bounds stand under the centre and end at or below the bottom.
	//! Some props have no collision above their base on any trace layer (BoxWooden_01: a ray went straight
	//! through it and called the crate stacked on it floating); what the eye sees is the prop's top.
	//! -1000 when there is none within 3 m.
	static float BoundsSupport(IEntity self, vector point, out string supportLabel)
	{
		s_ColumnSelf = self;
		s_vColumn = point;
		s_fColumnTop = -1000;
		s_ColumnBest = null;
		GetGame().GetWorld().QueryEntitiesByAABB(s_vColumn - "0.01 3 0.01", s_vColumn + "0.01 0.05 0.01", CollectColumn, null, EQueryEntitiesFlags.ALL);
		supportLabel = Label(s_ColumnBest);
		float top = s_fColumnTop;
		s_ColumnSelf = null;
		s_ColumnBest = null;
		return top;
	}

	//------------------------------------------------------------------------------------------------
	//! The surface under a point of the entity's bottom: the higher of the traced floor (SupportAt from
	//! 0.3 m above) and a prop top under it (BoundsSupport).
	static float SurfaceUnder(IEntity self, vector point, out string supportLabel)
	{
		float support = SupportAt(self, point[0], point[2], point[1] + 0.3, supportLabel);
		string boundsLabel;
		float top = BoundsSupport(self, point, boundsLabel);
		if (top > support)
		{
			support = top;
			supportLabel = boundsLabel;
		}

		return support;
	}

	//------------------------------------------------------------------------------------------------
	//! Clips: a surface of another entity passes through this entity's own volume. A building's overall
	//! bounds say nothing about where its walls are, so rays cross the bounds (shrunk 0.15 m) on all
	//! three axes from both sides and keep hits that land inside. A single graze does not count.
	static string Clips(vector mins, vector maxs, IEntity self)
	{
		s_sClipNames = "";
		s_iClipCount = 0;
		vector lo = mins + "0.15 0.15 0.15";
		vector hi = maxs - "0.15 0.15 0.15";
		if (hi[0] - lo[0] < 0.1 || hi[1] - lo[1] < 0.1 || hi[2] - lo[2] < 0.1)
			return "none";

		array<string> hitNames = {};
		array<int> hitCounts = {};
		for (int axis = 0; axis < 3; axis++)
		{
			int a1 = (axis + 1) % 3;
			int a2 = (axis + 2) % 3;
			for (int i = 0; i < 3; i++)
			{
				for (int j = 0; j < 3; j++)
				{
					for (int side = 0; side < 2; side++)
					{
						if (s_iClipCount >= 3)
							return s_sClipNames;

						vector from = lo;
						from[a1] = lo[a1] + (hi[a1] - lo[a1]) * (i + 0.5) / 3;
						from[a2] = lo[a2] + (hi[a2] - lo[a2]) * (j + 0.5) / 3;
						vector to = from;
						if (side == 0)
						{
							from[axis] = mins[axis] - 0.4;
							to[axis] = maxs[axis] + 0.4;
						}
						else
						{
							from[axis] = maxs[axis] + 0.4;
							to[axis] = mins[axis] - 0.4;
						}

						IEntity hit;
						float total = vector.Distance(from, to);
						float d = Trace(from, to, self, hit);
						if (!hit || d >= total - 0.01)
							continue;
						if (IsOwnPart(self, hit) || ChimeraCharacter.Cast(hit))
							continue;

						// The terrain and other huge entities (roads, ground plates) pass through anything sunk: not a clip.
						vector omin;
						vector omax;
						hit.GetWorldBounds(omin, omax);
						if (omax[0] - omin[0] > 60 || omax[2] - omin[2] > 60)
							continue;

						vector p = from + (to - from) * (d / total);
						if (p[0] < lo[0] || p[1] < lo[1] || p[2] < lo[2] || p[0] > hi[0] || p[1] > hi[1] || p[2] > hi[2])
							continue;

						string label = Label(hit);
						if (label.Contains("Terrain"))
							continue;

						int at = hitNames.Find(label);
						if (at < 0)
						{
							hitNames.Insert(label);
							hitCounts.Insert(1);
							continue;
						}

						hitCounts[at] = hitCounts[at] + 1;
						if (hitCounts[at] != 2)
							continue;

						if (!s_sClipNames.IsEmpty())
							s_sClipNames = s_sClipNames + ";";

						s_sClipNames = s_sClipNames + label;
						s_iClipCount++;
					}
				}
			}
		}

		if (s_sClipNames.IsEmpty())
			return "none";

		return s_sClipNames;
	}

	//------------------------------------------------------------------------------------------------
	//! Distance from the entity's horizontal bounds edge (capped at 4 m out from its centre) to the
	//! nearest spot a character could stand, on any side: a floor within 0.7 m of the entity's own
	//! floor with 1.4 m of clear air above it. 99 when none within 3 m.
	static float WalkDistance(vector mins, vector maxs, float floorY, IEntity self)
	{
		float cx = (mins[0] + maxs[0]) * 0.5;
		float cz = (mins[2] + maxs[2]) * 0.5;
		float rim = Math.Min(Math.Max(maxs[0] - mins[0], maxs[2] - mins[2]) * 0.5, 4.0);
		array<float> offsets = {0.5, 1.0, 1.5, 2.0, 3.0};
		IEntity hit;
		foreach (float offset : offsets)
		{
			float radius = rim + offset;
			for (int i = 0; i < 16; i++)
			{
				float angle = i * 22.5 * Math.DEG2RAD;
				float px = cx + Math.Cos(angle) * radius;
				float pz = cz + Math.Sin(angle) * radius;
				float floorDist = Trace(Vector(px, floorY + 1.0, pz), Vector(px, floorY - 1.0, pz), self, hit);
				// The terrain is not always on the trace's physics layer: it is the floor when the trace found none, or a higher one.
				float surface = GetGame().GetWorld().GetSurfaceY(px, pz);
				float standY = surface;
				if (floorDist < 2.0)
					standY = Math.Max(floorY + 1.0 - floorDist, surface);
				if (Math.AbsFloat(standY - floorY) > 0.7)
					continue;

				vector feet = Vector(px, standY, pz);
				float clear = Trace(feet + "0 0.35 0", feet + "0 1.75 0", self, hit);
				// 1.4 m of air, with float slack: a clear 1.4 m trace reports 1.3999999.
				if (clear < 1.35)
					continue;

				return offset;
			}
		}

		return 99;
	}

	//------------------------------------------------------------------------------------------------
	//! Fills the oriented box: local bounds through the world transform, scale applied once whether or
	//! not the transform axes carry it.
	static void OrientedBox(IEntity entity, LF_PlacementMeasure m)
	{
		vector lmin;
		vector lmax;
		entity.GetBounds(lmin, lmax);
		vector mat[4];
		entity.GetWorldTransform(mat);
		float scale = entity.GetScale();
		m.m_vAxis0 = mat[0].Normalized();
		m.m_vAxis1 = mat[1].Normalized();
		m.m_vAxis2 = mat[2].Normalized();
		vector mid = (lmin + lmax) * 0.5;
		m.m_vHalf = (lmax - lmin) * 0.5 * scale;
		m.m_vCenter = mat[3] + m.m_vAxis0 * (mid[0] * scale) + m.m_vAxis1 * (mid[1] * scale) + m.m_vAxis2 * (mid[2] * scale);
	}

	//------------------------------------------------------------------------------------------------
	//! A world point of the oriented box from local signs (-1..1 per axis).
	static vector BoxPoint(LF_PlacementMeasure m, float sx, float sy, float sz)
	{
		return m.m_vCenter + m.m_vAxis0 * (m.m_vHalf[0] * sx) + m.m_vAxis1 * (m.m_vHalf[1] * sy) + m.m_vAxis2 * (m.m_vHalf[2] * sz);
	}

	//------------------------------------------------------------------------------------------------
	//! Bottom corners (inset OVERHANG_INSET of the footprint) that have no surface within OVERHANG_DROP
	//! under them while the entity's centre does: half on a table, half in the air.
	static void Overhang(IEntity entity, LF_PlacementMeasure m)
	{
		m.m_iOverhang = 0;
		m.m_fOverDrop = 0;
		if (m.m_vHalf[0] < 0.05 || m.m_vHalf[2] < 0.05)
			return;

		float k = 1 - OVERHANG_INSET * 2;
		for (int c = 0; c < 4; c++)
		{
			float sx = -k;
			if (c == 1 || c == 2)
				sx = k;
			float sz = -k;
			if (c >= 2)
				sz = k;

			vector p = BoxPoint(m, sx, -1, sz);
			string label;
			float support = SurfaceUnder(entity, p, label);
			float drop = p[1] - support;
			if (drop > OVERHANG_DROP)
			{
				m.m_iOverhang++;
				if (drop > m.m_fOverDrop)
					m.m_fOverDrop = drop;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Something within 0.3 m of a vertical side (traced out from the centre along the box's horizontal axes).
	static bool Backed(IEntity entity, LF_PlacementMeasure m)
	{
		array<vector> dirs = {m.m_vAxis0, m.m_vAxis0 * -1, m.m_vAxis2, m.m_vAxis2 * -1};
		array<float> reach = {m.m_vHalf[0], m.m_vHalf[0], m.m_vHalf[2], m.m_vHalf[2]};
		for (int i = 0; i < 4; i++)
		{
			IEntity hit;
			float len = reach[i] + 0.3;
			float d = SurfaceTrace(m.m_vCenter, m.m_vCenter + dirs[i] * len, entity, hit);
			if (d < len - 0.01 && hit && !IsOwnPart(entity, hit))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Every check on one entity. `withWalk` adds the reach scan (interactables only: it is the costly one).
	static LF_PlacementMeasure Measure(IEntity entity, bool withWalk)
	{
		LF_PlacementMeasure m = new LF_PlacementMeasure();
		if (!entity || !GetGame() || !GetGame().GetWorld())
			return m;

		entity.GetWorldBounds(m.m_vMins, m.m_vMaxs);
		float cx = (m.m_vMins[0] + m.m_vMaxs[0]) * 0.5;
		float cz = (m.m_vMins[2] + m.m_vMaxs[2]) * 0.5;
		string label;
		float support = SupportAt(entity, cx, cz, m.m_vMins[1] + 0.5, label);
		m.m_sSupport = label;
		string boundsLabel;
		float boundsTop = BoundsSupport(entity, Vector(cx, m.m_vMins[1], cz), boundsLabel);
		if (boundsTop > support)
		{
			support = boundsTop;
			m.m_sSupport = boundsLabel;
		}

		m.m_fGap = m.m_vMins[1] - support;
		m.m_fPivotGap = entity.GetOrigin()[1] - support;

		vector mat[4];
		entity.GetWorldTransform(mat);
		vector up = mat[1].Normalized();
		float upY = Math.Clamp(up[1], -1, 1);
		m.m_fTilt = Math.Acos(upY) * Math.RAD2DEG;
		m.m_fScale = entity.GetScale();

		OrientedBox(entity, m);
		m.m_bCharacter = ChimeraCharacter.Cast(entity) != null;
		if (m.m_bCharacter)
		{
			// A server-side NPC is not animated: its mesh bounds are the bind pose, arms out.
			m.m_vHalf = "0.3 0.9 0.3";
			m.m_vCenter = entity.GetOrigin() + m.m_vAxis1 * 0.9;
			m.m_vMins = entity.GetOrigin() - "0.3 0 0.3";
			m.m_vMaxs = entity.GetOrigin() + "0.3 1.8 0.3";
			m.m_iOverhang = 0;
			if (withWalk)
				m.m_fWalk = WalkDistance(m.m_vMins, m.m_vMaxs, support, entity);

			return m;
		}

		m.m_bPoint = m.m_vHalf[0] < 0.01 && m.m_vHalf[1] < 0.01 && m.m_vHalf[2] < 0.01;
		if (!m.m_bPoint)
		{
			Overhang(entity, m);
			m.m_bBacked = Backed(entity, m);
			m.m_sClips = Clips(m.m_vMins, m.m_vMaxs, entity);
		}

		if (withWalk)
			m.m_fWalk = WalkDistance(m.m_vMins, m.m_vMaxs, support, entity);

		return m;
	}

	//------------------------------------------------------------------------------------------------
	//! The physical problems, comma separated ("" when none): floating, sunk, overhang, scaled, tilted.
	//! Context the Node side knows (a hung sign, a sack laid down on purpose) is applied there.
	static string Flags(LF_PlacementMeasure m)
	{
		array<string> flags = {};
		if (m.m_bPoint)
			return "";

		if (m.m_fGap > GAP)
			flags.Insert("floating");
		// Sunk: the bounds bottom and the pivot are both under the floor. Mesh that reaches below a pivot
		// standing on the floor (vehicle wheels) and a pivot left on the floor under a raised mesh (a vice
		// on a bench) are modelled that way, not buried.
		if (Math.Max(m.m_fGap, m.m_fPivotGap) < -GAP)
			flags.Insert("sunk");
		if (m.m_iOverhang > 0 && m.m_fGap <= GAP)
			flags.Insert("overhang");
		if (m.m_fScale < SCALE_MIN || m.m_fScale > SCALE_MAX)
			flags.Insert("scaled");
		if (m.m_fTilt > TILT)
			flags.Insert("tilted");

		string text;
		foreach (int i, string flag : flags)
		{
			if (i > 0)
				text = text + ",";
			text = text + flag;
		}

		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected static string F(float value)
	{
		return string.Format("%1", value);
	}

	//------------------------------------------------------------------------------------------------
	protected static string V(vector v)
	{
		return string.Format("%1,%2,%3", v[0], v[1], v[2]);
	}

	//------------------------------------------------------------------------------------------------
	//! key=value text, one token per field (agent-box/shots.mjs parseEnt, agent-box/place-audit.mjs).
	static string Format(LF_PlacementMeasure m)
	{
		string text = string.Format("bmin=%1 bmax=%2 gap=%3 tilt=%4 walk=%5", V(m.m_vMins), V(m.m_vMaxs), F(m.m_fGap), F(m.m_fTilt), F(m.m_fWalk));
		text = text + string.Format(" pgap=%1 clip=%2 scale=%3 on=%4 over=%5 drop=%6", F(m.m_fPivotGap), m.m_sClips, F(m.m_fScale), m.m_sSupport, m.m_iOverhang, F(m.m_fOverDrop));
		int backed = 0;
		if (m.m_bBacked)
			backed = 1;
		if (m.m_bPoint)
			text = text + " point=1";
		if (m.m_bCharacter)
			text = text + " char=1";
		text = text + string.Format(" backed=%1 c=%2 h=%3 a0=%4 a1=%5 a2=%6", backed, V(m.m_vCenter), V(m.m_vHalf), V(m.m_vAxis0), V(m.m_vAxis1), V(m.m_vAxis2));
		string flags = Flags(m);
		if (flags.IsEmpty())
			flags = "none";
		return text + " flags=" + flags;
	}

	//------------------------------------------------------------------------------------------------
	protected static string s_sFindName;
	protected static string s_sFindPrefab;
	protected static vector s_vFindPos;
	protected static IEntity s_FindBest;
	protected static float s_fFindScore;

	protected static bool CollectFind(IEntity entity)
	{
		if (!entity || (ChimeraCharacter.Cast(entity) && entity.GetName().IsEmpty()))
			return true;

		typename type = entity.Type();
		if (type.IsInherited(GenericTerrainEntity) || type.IsInherited(GenericWorldEntity))
			return true;

		float score = vector.Distance(entity.GetOrigin(), s_vFindPos);
		if (!s_sFindName.IsEmpty() && entity.GetName() == s_sFindName)
			score = score - 1000;
		else if (score > 1.0)
			return true;
		else if (!s_sFindPrefab.IsEmpty() && PrefabBase(entity) == s_sFindPrefab)
			score = score - 100;

		if (!s_FindBest || score < s_fFindScore)
		{
			s_FindBest = entity;
			s_fFindScore = score;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The world entity a layer placed at `pos`: the one named `name` within 1.5 m, else the nearest
	//! within 1 m (one of prefab `prefabBase` first). Null when nothing is there.
	static IEntity FindPlaced(string name, string prefabBase, vector pos)
	{
		s_sFindName = name;
		s_sFindPrefab = prefabBase;
		s_vFindPos = pos;
		s_FindBest = null;
		s_fFindScore = 0;
		GetGame().GetWorld().QueryEntitiesBySphere(pos, 1.5, CollectFind, null, EQueryEntitiesFlags.ALL);
		IEntity found = s_FindBest;
		s_FindBest = null;
		return found;
	}

	//------------------------------------------------------------------------------------------------
	protected static ref array<EntityID> s_aDoorIds;

	protected static bool CollectDoor(IEntity entity)
	{
		if (entity && entity.FindComponent(DoorComponent))
			s_aDoorIds.Insert(entity.GetID());

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One line per door leaf within `radius` of `center`: its oriented box and the building it belongs to.
	static void DoorLines(vector center, float radius, notnull array<string> lines)
	{
		s_aDoorIds = {};
		BaseWorld world = GetGame().GetWorld();
		world.QueryEntitiesBySphere(center, radius, CollectDoor, null, EQueryEntitiesFlags.ALL);
		foreach (EntityID id : s_aDoorIds)
		{
			IEntity door = world.FindEntityByID(id);
			if (!door)
				continue;

			LF_PlacementMeasure m = new LF_PlacementMeasure();
			OrientedBox(door, m);
			IEntity root = door.GetRootParent();
			if (!root)
				root = door;
			lines.Insert(string.Format("door name=%1 root=%2 rp=%3 c=%4 h=%5 a0=%6 a1=%7 a2=%8", Label(door), Label(root), V(root.GetOrigin()), V(m.m_vCenter), V(m.m_vHalf), V(m.m_vAxis0), V(m.m_vAxis1), V(m.m_vAxis2)));
		}

		s_aDoorIds = null;
	}

	//------------------------------------------------------------------------------------------------
	//! One probe ray for the placement tool (tools/place/place.mjs): the first surface between `from` and
	//! `to` on any physics layer, characters skipped. "ray <id> d=<m> len=<m> hit=<label> root=<label>
	//! p=<hit point, or `to` when clear> sy=<terrain height under p>". The terrain is not on every trace
	//! layer, so a caller looking for a floor takes the higher of p's height and sy. `exclude` (and its
	//! children) is traced through: the prop being re-placed must not answer its own probe.
	static string RayLine(string id, vector from, vector to, IEntity exclude = null)
	{
		float len = vector.Distance(from, to);
		float d = len;
		IEntity hit;
		if (len > 0.001)
		{
			vector dir = (to - from) * (1 / len);
			vector start = from;
			float done;
			for (int n = 0; n < 4; n++)
			{
				float step = SurfaceTrace(start, to, exclude, hit);
				if (!hit || (!ChimeraCharacter.Cast(hit) && !(exclude && IsOwnPart(exclude, hit))))
				{
					d = done + step;
					break;
				}

				// A standing NPC is not a wall or a floor: go on behind it (0.6 m); a child of the excluded prop: just past the hit.
				float skip = 0.6;
				if (!ChimeraCharacter.Cast(hit))
					skip = 0.05;

				done = done + step + skip;
				hit = null;
				if (done >= len)
					break;

				start = from + dir * done;
			}

			if (d > len)
				d = len;
		}

		vector p = to;
		if (len > 0.001)
			p = from + (to - from) * (d / len);

		IEntity root = hit;
		if (hit && hit.GetRootParent())
			root = hit.GetRootParent();

		float sy = GetGame().GetWorld().GetSurfaceY(p[0], p[2]);
		return string.Format("ray %1 d=%2 len=%3 hit=%4 root=%5 p=%6 sy=%7", id, F(d), F(len), Label(hit), Label(root), V(p), F(sy));
	}

	//------------------------------------------------------------------------------------------------
	//! Measure every entity listed in `inPath` and write one line each to `outPath` (agent-box/place-audit.mjs).
	//! Input lines:
	//!   town <x> <z> <radius>                               also list the door leaves in that circle
	//!   <index> <name|-> <prefab base|-> <x> <y> <z> <walk 0|1>
	//!   ray <id> <x> <y> <z> <x2> <y2> <z2> [exclude name]  a probe ray (RayLine), not an entity row
	//! Output lines: "<index> missing=1", "<index> <Format> found=<label>", "door ...", "ray ...". Returns the
	//! entity rows written, or -1 when a file cannot be opened. `missing` counts entities not found.
	static int RunBatch(string inPath, string outPath, out int missing)
	{
		missing = 0;
		FileHandle input = FileIO.OpenFile(inPath, FileMode.READ);
		if (!input)
		{
			LF_Debug.Error("PlaceAudit", "cannot read " + inPath);
			return -1;
		}

		array<string> rows = {};
		int measured;
		string line;
		while (input.ReadLine(line) >= 0)
		{
			line.TrimInPlace();
			if (line.IsEmpty())
				continue;

			array<string> parts = {};
			line.Split(" ", parts, true);
			if (parts.Count() == 4 && parts[0] == "town")
			{
				DoorLines(Vector(parts[1].ToFloat(), 0, parts[2].ToFloat()), parts[3].ToFloat(), rows);
				continue;
			}

			if (parts[0] == "ray")
			{
				IEntity rayExclude;
				if (parts.Count() == 9)
					rayExclude = GetGame().GetWorld().FindEntityByName(parts[8]);

				if (parts.Count() >= 8)
					rows.Insert(RayLine(parts[1], Vector(parts[2].ToFloat(), parts[3].ToFloat(), parts[4].ToFloat()), Vector(parts[5].ToFloat(), parts[6].ToFloat(), parts[7].ToFloat()), rayExclude));

				continue;
			}

			if (parts.Count() < 7)
				continue;

			string name = parts[1];
			if (name == "-")
				name = "";
			string prefab = parts[2];
			if (prefab == "-")
				prefab = "";
			IEntity entity = FindPlaced(name, prefab, Vector(parts[3].ToFloat(), parts[4].ToFloat(), parts[5].ToFloat()));
			measured++;
			if (!entity)
			{
				missing++;
				rows.Insert(parts[0] + " missing=1");
				continue;
			}

			rows.Insert(parts[0] + " " + Format(Measure(entity, parts[6] == "1")) + " found=" + Label(entity));
		}

		input.Close();
		FileHandle output = FileIO.OpenFile(outPath, FileMode.WRITE);
		if (!output)
		{
			LF_Debug.Error("PlaceAudit", "cannot write " + outPath);
			return -1;
		}

		foreach (string row : rows)
			output.WriteLine(row);
		output.Close();
		return measured;
	}
}
