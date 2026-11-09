// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --tier all`: renaming MODE_NAMES entry "Mode1" to "Mode2"
// failed it with `[LFTEST] FAIL siren/mode-enum-integrity`, `- mode name Mode2 is unique`.
// Restored; green.

// tier: LOGIC
class LF_Test_SirenMode : LF_Test
{
	override string GetName()
	{
		return "siren/mode-enum-integrity";
	}

	override void Run(LF_TestContext ctx)
	{
		// RplSave/RplLoad send the mode as WriteIntRange(index, 0, N_MODES), so the count must
		// match the list the editor combo boxes use.
		ctx.True(LF_SirenMode.N_MODES > 0, "siren mode count is positive");
		ctx.Equal(LF_SirenMode.N_MODES, LF_SirenMode.MODE_NAMES.Count(), "N_MODES matches MODE_NAMES length");
		ctx.True(LF_SirenMode.SIREN_SOUNDS.Count() > 0, "siren sound list is not empty");

		// LF_SirenManagerComponent.OnPostInit falls back to index 0 when "default" is missing,
		// so "default" must be the first mode.
		if (LF_SirenMode.MODE_NAMES.Count() > 0)
		{
			ParamEnum first = LF_SirenMode.MODE_NAMES.Get(0);
			ctx.NotNull(first, "mode entry 0 is present");
			if (first)
				ctx.EqualStr("default", first.m_Key, "the first mode is the default mode");
		}

		// LF_SirenModes.Find resolves a mode by name, so every name is set and unique.
		array<string> seen = {};
		for (int i = 0; i < LF_SirenMode.MODE_NAMES.Count(); i++)
		{
			ParamEnum entry = LF_SirenMode.MODE_NAMES.Get(i);
			ctx.NotNull(entry, string.Format("mode entry %1 is present", i));
			if (!entry)
				continue;

			ctx.False(entry.m_Key.IsEmpty(), string.Format("mode entry %1 has a name", i));
			ctx.False(seen.Contains(entry.m_Key), string.Format("mode name %1 is unique", entry.m_Key));
			seen.Insert(entry.m_Key);
		}
	}
};
