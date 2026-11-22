//! Harness verb `clutterscan [x] [z] [radius]`: the island audit's clutter sweep (LF_PlacementAudit.ClutterLines) on a running
//! client, so what a player sees after the spawners have run (roadblocks, job props, a cash van's drop) is judged too, not only
//! the layers. Writes one `clutter ...` line per prop to $profile:clutter-scan.txt and answers with the count and every prop that
//! stands on a road in the open (place-runtime.mjs judges them as `road-clutter`). Defaults: the player's position, 450 m.
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "clutterscan")
			return super.Handle(cmd);

		vector center;
		float radius = 450;
		if (!cmd.Arg1.IsEmpty() && !cmd.Arg2.IsEmpty())
		{
			center = Vector(cmd.Arg1.ToFloat(), 0, cmd.Arg2.ToFloat());
			if (!cmd.Arg3.IsEmpty())
				radius = cmd.Arg3.ToFloat();
		}
		else
		{
			IEntity player = SCR_PlayerController.GetLocalControlledEntity();
			if (!player)
			{
				LF_AgentHarness.Answer(cmd.Id, "error", "no_player");
				return true;
			}

			center = player.GetOrigin();
			if (!cmd.Arg1.IsEmpty())
				radius = cmd.Arg1.ToFloat();
		}

		array<string> lines = {};
		LF_PlacementAudit.ClutterLines(center, radius, lines);
		string path = "$profile:clutter-scan.txt";
		FileHandle file = FileIO.OpenFile(path, FileMode.WRITE);
		if (!file)
		{
			LF_Debug.Error("PlaceAudit", "clutterscan: cannot write " + path);
			LF_AgentHarness.Answer(cmd.Id, "error", "cannot_write");
			return true;
		}

		string onRoad;
		int roads;
		foreach (string line : lines)
		{
			file.WriteLine(line);
			if (line.Contains(" on=RoadEntity") && line.Contains(" roofed=0"))
			{
				roads++;
				if (roads <= 12)
					onRoad += " | " + line;
			}
		}

		file.Close();
		LF_Debug.Info("PlaceAudit", string.Format("clutterscan: %1 clutter props, %2 on a road, within %3 m of %4", lines.Count(), roads, radius, center));
		LF_AgentHarness.Answer(cmd.Id, "ok", string.Format("props=%1 onroad=%2 file=%3%4", lines.Count(), roads, path, onRoad));
		return true;
	}
}
