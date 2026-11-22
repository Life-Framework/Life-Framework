//------------------------------------------------------------------------------------------------
//! One stop of a job run: the world entity a player goes to (the pack names it; the core holds no names)
//! and a short label for the hover card ("furnace").
[BaseContainerProps()]
class LF_MapRunStop
{
	[Attribute("", UIWidgets.EditBox, "Name of the world entity at this stop (a desk, a till, a rock, a buyer)")]
	string m_sEntity;

	[Attribute("", UIWidgets.EditBox, "Localization key of the stop's short name, e.g. #LF-MapRunStop_Furnace")]
	string m_sLabel;

	//------------------------------------------------------------------------------------------------
	static LF_MapRunStop Make(string entity, string label)
	{
		LF_MapRunStop stop = new LF_MapRunStop();
		stop.m_sEntity = entity;
		stop.m_sLabel = label;
		return stop;
	}
}

//------------------------------------------------------------------------------------------------
//! One job run as a chain of stops in order (mining: desk, pickaxe, rocks, furnace, buyer). Pack data.
[BaseContainerProps()]
class LF_MapRun : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Run id, e.g. mining")]
	string m_sId;

	[Attribute("", UIWidgets.EditBox, "Localization key of the run's name, e.g. #LF-MapRun_Mining (\"Mining run\")")]
	string m_sLabel;

	[Attribute("", UIWidgets.Object, "The stops in the order a player does them")]
	ref array<ref LF_MapRunStop> m_aStops;

	//------------------------------------------------------------------------------------------------
	static LF_MapRun Make(string id, string label = "")
	{
		LF_MapRun run = new LF_MapRun();
		run.m_sId = id;
		run.m_sLabel = label;
		run.m_aStops = {};
		return run;
	}

	//------------------------------------------------------------------------------------------------
	int StopCount()
	{
		if (!m_aStops)
			return 0;

		return m_aStops.Count();
	}
}

//------------------------------------------------------------------------------------------------
//! The run table: Configs/Map/LF_MapRuns.conf (empty in the core) with each pack overlay over it.
[BaseContainerProps(configRoot: true)]
class LF_MapRunConfig
{
	static const string FEATURE = "MapRuns";
	static const ResourceName CONFIG_PATH = "{7B3E91C2A45D60F8}Configs/Map/LF_MapRuns.conf";

	[Attribute("", UIWidgets.Object, "One row per job run the map can draw")]
	ref array<ref LF_MapRun> m_aRuns;

	protected static ref LF_MapRunConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered table, or null (logged once) when the core file does not resolve.
	static LF_MapRunConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("run table does not resolve, the map draws no job runs: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_MapRunConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
		{
			LF_Debug.Error(FEATURE, string.Format("run table has an unexpected root class: %1", CONFIG_PATH));
			return null;
		}

		s_Config.ApplyLayers();
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay of this class, in load order, over this instance.
	void ApplyLayers()
	{
		if (!m_aRuns)
			m_aRuns = new array<ref LF_MapRun>();

		LF_MapRunList list = new LF_MapRunList(this);
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_MapRunConfig, "mapRuns", resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = LF_ConfigOverlay.ApplyContainerList(list, resource.GetResource().ToBaseContainer(), "m_aRuns", names[i], claims);
			LF_Debug.Info(FEATURE, string.Format("pack overlay %1 changed %2 run(s)", names[i], changed));
		}
	}
}

//------------------------------------------------------------------------------------------------
//! The run table's `m_aRuns` list for the shared merge (LF_ConfigOverlay), keyed by run id.
class LF_MapRunList : LF_ConfigList
{
	protected LF_MapRunConfig m_Config;

	void LF_MapRunList(LF_MapRunConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "mapRuns.runs";
	}

	override int Count()
	{
		return m_Config.m_aRuns.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aRuns[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_MapRun.Cast(record).m_sId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sId", id);
		return id;
	}

	override string IdHint()
	{
		return "a run id";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		return LF_MapRun.Make(id);
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_MapRun run = LF_MapRun.Cast(record);
		if (index < m_Config.m_aRuns.Count())
			m_Config.m_aRuns.Set(index, run);
		else
			m_Config.m_aRuns.Insert(run);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aRuns.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_MapRun run = LF_MapRun.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_sLabel") && container.Get("m_sLabel", run.m_sLabel))
			changed++;

		if (container.IsVariableSetDirectly("m_aStops"))
		{
			BaseContainerList stops = container.GetObjectArray("m_aStops");
			run.m_aStops = {};
			for (int i = 0; stops && i < stops.Count(); i++)
			{
				LF_MapRunStop stop = LF_MapRunStop.Cast(BaseContainerTools.CreateInstanceFromContainer(stops.Get(i)));
				if (stop)
					run.m_aStops.Insert(stop);
			}

			changed++;
		}

		return changed;
	}
}

//------------------------------------------------------------------------------------------------
//! Where one run's stop is on this machine's map, if it shows: resolved once per map open.
class LF_MapRunPoint
{
	int m_iRun;
	int m_iStep;
	vector m_vPos;
	bool m_bFound;
	bool m_bShown;
}

//------------------------------------------------------------------------------------------------
//! One run a badge belongs to: which run, and which step the badge is.
class LF_MapRunMatch
{
	int m_iRun;
	int m_iStep;

	static LF_MapRunMatch Make(int run, int step)
	{
		LF_MapRunMatch m = new LF_MapRunMatch();
		m.m_iRun = run;
		m.m_iStep = step;
		return m;
	}
}

//------------------------------------------------------------------------------------------------
//! The pure rules of the run chains: which runs a badge belongs to, which stops a player's map may show,
//! the card line and the dashes. No world: the LOGIC tests feed positions.
class LF_MapRunRules
{
	//! A badge belongs to a stop whose entity stands within this many metres of the place.
	static const float MATCH_RADIUS = 45; // lf-fixed: a stop entity to its place's badge
	//! Only the runs whose stop is the badge's nearest, give or take this much: a shop next door (the timber
	//! merchant beside the hardware store) is not this badge's stop. A stop two runs share is equally near.
	static const float NEAREST_SLACK = 10; // lf-fixed: metres
	//! A stop shows only near a place the player's map already holds (no undiscovered place leaks).
	static const float SHOW_RADIUS = 80; // lf-fixed: a stop to a known place
	//! Dash and gap of a connector line in screen pixels.
	static const float DASH = 9; // lf-fixed: drawing
	static const float GAP = 6; // lf-fixed: drawing

	//------------------------------------------------------------------------------------------------
	//! Every (run, step) whose stop is within MATCH_RADIUS of `badge`, nearest stop per run. Points must be
	//! found and shown. \return how many matched.
	static int Match(vector badge, notnull array<ref LF_MapRunPoint> points, notnull array<ref LF_MapRunMatch> into)
	{
		into.Clear();
		map<int, float> best = new map<int, float>();
		map<int, int> step = new map<int, int>();
		float reach = MATCH_RADIUS * MATCH_RADIUS;
		foreach (LF_MapRunPoint point : points)
		{
			if (!point.m_bFound || !point.m_bShown)
				continue;

			float d = vector.DistanceSqXZ(point.m_vPos, badge);
			if (d > reach)
				continue;

			float had;
			if (best.Find(point.m_iRun, had) && had <= d)
				continue;

			best.Set(point.m_iRun, d);
			step.Set(point.m_iRun, point.m_iStep);
		}

		float nearest = reach;
		foreach (int run, float d : best)
		{
			if (d < nearest)
				nearest = d;
		}

		float keep = Math.Sqrt(nearest) + NEAREST_SLACK;
		foreach (int run, int s : step)
		{
			if (best.Get(run) <= keep * keep)
				into.Insert(LF_MapRunMatch.Make(run, s));
		}

		return into.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Marks each found point shown when a known place (`known` positions) is within SHOW_RADIUS.
	static void MarkShown(notnull array<ref LF_MapRunPoint> points, notnull array<vector> known)
	{
		float reach = SHOW_RADIUS * SHOW_RADIUS;
		foreach (LF_MapRunPoint point : points)
		{
			point.m_bShown = false;
			if (!point.m_bFound)
				continue;

			foreach (vector at : known)
			{
				if (vector.DistanceSqXZ(point.m_vPos, at) <= reach)
				{
					point.m_bShown = true;
					break;
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The card line's key and arguments: "%1: step %2 of %3, next: %4", or the last-stop form.
	static string CardKey(int step, int count, out string stepText, out string countText)
	{
		stepText = (step + 1).ToString();
		countText = count.ToString();
		if (step + 1 >= count)
			return "#LF-MapRun_CardLast";

		return "#LF-MapRun_Card";
	}

	//------------------------------------------------------------------------------------------------
	//! Dash segments (x0, y0, x1, y1, ...) along a line in screen pixels.
	static int Dashes(float x0, float y0, float x1, float y1, notnull array<float> into)
	{
		float dx = x1 - x0;
		float dy = y1 - y0;
		float length = Math.Sqrt(dx * dx + dy * dy);
		if (length < 1)
			return 0;

		float ux = dx / length;
		float uy = dy / length;
		int added = 0;
		for (float t = 0; t < length && added < 400; t += DASH + GAP)
		{
			float end = t + DASH;
			if (end > length)
				end = length;

			into.Insert(x0 + ux * t);
			into.Insert(y0 + uy * t);
			into.Insert(x0 + ux * end);
			into.Insert(y0 + uy * end);
			added++;
		}

		return added;
	}
}

//------------------------------------------------------------------------------------------------
//! The run chains on this machine: every stop of every run resolved to a world position by its entity
//! name (layer entities exist on every client). Read once per map open.
class LF_MapRuns
{
	//------------------------------------------------------------------------------------------------
	//! Resolves every stop of `config` into `into`, one point per stop. \return how many were found.
	static int Resolve(notnull LF_MapRunConfig config, notnull array<ref LF_MapRunPoint> into)
	{
		into.Clear();
		if (!config.m_aRuns || !GetGame() || !GetGame().GetWorld())
			return 0;

		int found = 0;
		foreach (int r, LF_MapRun run : config.m_aRuns)
		{
			if (!run || !run.m_aStops)
				continue;

			foreach (int s, LF_MapRunStop stop : run.m_aStops)
			{
				LF_MapRunPoint point = new LF_MapRunPoint();
				point.m_iRun = r;
				point.m_iStep = s;
				IEntity entity;
				if (stop && !stop.m_sEntity.IsEmpty())
					entity = GetGame().GetWorld().FindEntityByName(stop.m_sEntity);

				if (entity)
				{
					point.m_vPos = entity.GetOrigin();
					point.m_bFound = true;
					found++;
				}

				into.Insert(point);
			}
		}

		return found;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_MapRunConfig's statics (LF_StaticReset finds it by reflection).
class LF_MapRunConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_MapRunConfig";
	}

	override void Apply()
	{
		LF_MapRunConfig.ResetStatics();
	}
}
