// red-proof: observed 2026-11-23 in the same batched --red run: RedProof() asks for slot 0, so every `flag N has a usable sample prefab` fails, RED-OK; green without the hook.
//! #4449: under the Everon 1989 header (the pack leg), each of the 20 flag slots gang_flag_01 to
//! gang_flag_20 resolves to a loadable prefab through the layered slot config, and the pack gives each
//! flag its real name. The registry flag wall (LF_OrgFlagWall) and the gang pole slice reuse the slots.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronFlagWallSlots : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/flag-wall-slots";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_OrgFlagWall.ResetStatics();
		array<string> seen = {};
		for (int flag = 1; flag <= LF_OrgFlags.COUNT; flag++)
		{
			int ask = flag;
			if (RedProof())
				ask = 0;

			ResourceName prefab = LF_OrgFlagWall.SamplePrefab(ask);
			ctx.False(prefab.IsEmpty(), string.Format("flag %1 has a usable sample prefab", flag));
			if (!prefab.IsEmpty())
			{
				ctx.True(seen.Find(prefab) == -1, string.Format("flag %1 has its own prefab", flag));
				seen.Insert(prefab);
			}

			string name = WidgetManager.Translate(LF_OrgFlagWall.NameKey(flag));
			ctx.False(name.IsEmpty() || name.StartsWith("Flag "), string.Format("flag %1 has a real name in the pack, got '%2'", flag, name));
		}

		LF_OrgFlagWall.ResetStatics();
	}
}
