// Harness verb `flocks [radius]` (client side, read-only; #4403): what THIS client holds of the vanilla bird
// flocks (`SCR_Bird`) around the local character, and of the single birds a flock spawns. Replies
//   world=<AIWorld class|none> flocks=N birds=M | flock=<prefab>:d=<m>:vis=<0|1>:kids=<n>:rpl=<0|1> ...
// so a proof can tell a flock that never reached the client from a flock with no visible birds.

//------------------------------------------------------------------------------------------------
class LF_FlockProbe
{
	static const float DEFAULT_RADIUS = 300; // lf-fixed: a read-only harness default
	static const float MAX_RADIUS = 3000; // lf-fixed: harness query cap
	static const int MAX_ENTRIES = 6; // lf-fixed: harness line length cap

	protected static vector s_vOrigin;
	protected static int s_iFlocks;
	protected static int s_iBirds;
	protected static int s_iBirdsVisible;
	protected static ref array<string> s_aFlocks;
	protected static array<string> LzFlocks()
	{
		if (!s_aFlocks)
			s_aFlocks = new array<string>();
		return s_aFlocks;
	}
	protected static ref array<string> s_aBirds;
	protected static array<string> LzBirds()
	{
		if (!s_aBirds)
			s_aBirds = new array<string>();
		return s_aBirds;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Collect(IEntity entity)
	{
		if (!entity)
			return true;

		string prefab = LF_Utils.GetPrefabName(entity);
		SCR_Bird flock = SCR_Bird.Cast(entity);
		if (flock)
		{
			s_iFlocks++;
			LzFlocks();
			if (s_aFlocks.Count() >= MAX_ENTRIES)
				return true;

			int vis = 0;
			if (entity.GetFlags() & EntityFlags.VISIBLE)
				vis = 1;

			int kids = 0;
			IEntity child = entity.GetChildren();
			while (child)
			{
				kids++;
				child = child.GetSibling();
			}

			int rpl = 0;
			if (entity.FindComponent(RplComponent))
				rpl = 1;

			float dist = vector.Distance(s_vOrigin, entity.GetOrigin());
			LzFlocks();
			s_aFlocks.Insert(string.Format("%1:d=%2:vis=%3:kids=%4:rpl=%5", LF_HarnessSeen.Short(prefab), Math.Round(dist * 10) / 10, vis, kids, rpl));
			return true;
		}

		if (prefab.Contains("Columba_livia_domestica.et") || prefab.Contains("Larus_argentatus.et"))
		{
			s_iBirds++;
			int birdVis = 0;
			if (entity.GetFlags() & EntityFlags.VISIBLE)
			{
				birdVis = 1;
				s_iBirdsVisible++;
			}

			LzBirds();
			if (s_aBirds.Count() >= MAX_ENTRIES && birdVis == 0)
				return true;
			LzBirds();
			if (s_aBirds.Count() >= 2 * MAX_ENTRIES)
				return true;

			int mesh = 0;
			if (entity.GetVObject())
				mesh = 1;

			int parent = 0;
			if (entity.GetParent())
				parent = 1;

			vector at = entity.GetOrigin();
			float ground = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
			float birdDist = vector.Distance(s_vOrigin, at);
			LzBirds();
			s_aBirds.Insert(string.Format("%1:d=%2:above=%3:vis=%4:mesh=%5:parent=%6:cls=%7:flags=%8", LF_HarnessSeen.Short(prefab), Math.Round(birdDist * 10) / 10, Math.Round((at[1] - ground) * 10) / 10, birdVis, mesh, parent, entity.ClassName(), entity.GetFlags()));
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static string Report(float radius)
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
			return "error no_character";

		if (radius <= 0)
			radius = DEFAULT_RADIUS;

		radius = Math.Min(radius, MAX_RADIUS);
		s_vOrigin = character.GetOrigin();
		s_iFlocks = 0;
		s_iBirds = 0;
		s_iBirdsVisible = 0;
		LzFlocks();
		s_aFlocks.Clear();
		LzBirds();
		s_aBirds.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(s_vOrigin, radius, Collect, null, EQueryEntitiesFlags.ALL);

		string world = "none";
		AIWorld aiWorld = GetGame().GetAIWorld();
		if (aiWorld)
			world = aiWorld.ClassName();

		string listed = "none";
		LzFlocks();
		for (int i = 0; i < s_aFlocks.Count(); i++)
		{
			if (i == 0)
				listed = LzFlocks()[i];
			else
				listed += " | " + LzFlocks()[i];
		}

		string birds = "none";
		LzBirds();
		for (int j = 0; j < s_aBirds.Count(); j++)
		{
			if (j == 0)
				birds = LzBirds()[j];
			else
				birds += " | " + LzBirds()[j];
		}

		return string.Format("world=%1 r=%2 at=%3 flocks=%4 birds=%5 birdsVisible=%8 | %6 || %7", world, radius, s_vOrigin, s_iFlocks, s_iBirds, listed, birds, s_iBirdsVisible);
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "flocks")
			return super.Handle(cmd);

		string text = LF_FlockProbe.Report(cmd.Arg1.ToFloat());
		if (text.StartsWith("error"))
		{
			LF_AgentHarness.Answer(cmd.Id, "error", text);
			return true;
		}

		LF_AgentHarness.Answer(cmd.Id, "ok", text);
		return true;
	}
}
