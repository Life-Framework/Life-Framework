// red-proof: PENDING for this revision - not yet observed (the server install was being restored).
// Planned perturbation: give two EL_SirenMode.MODE_NAMES entries the same key, or move
// "default" off index 0, and run `tools\cli test --no-build --tier fast`; the unique-name or
// default-first assertion goes red. The old assertion compared N_MODES with a list the test
// typed itself, which restated the constant and was removed.

// tier: LOGIC
class EL_Test_SirenMode : EL_Test
{
	override string GetName()
	{
		return "siren/mode-enum-integrity";
	}

	override void Run(EL_TestContext ctx)
	{
		// RplSave/RplLoad send the mode as WriteIntRange(index, 0, N_MODES), so the count must
		// match the list the editor combo boxes use.
		ctx.True(EL_SirenMode.N_MODES > 0, "siren mode count is positive");
		ctx.Equal(EL_SirenMode.N_MODES, EL_SirenMode.MODE_NAMES.Count(), "N_MODES matches MODE_NAMES length");
		ctx.True(EL_SirenMode.SIREN_SOUNDS.Count() > 0, "siren sound list is not empty");

		// EL_SirenManagerComponent.OnPostInit falls back to index 0 when "default" is missing,
		// so "default" must be the first mode.
		if (EL_SirenMode.MODE_NAMES.Count() > 0)
		{
			ParamEnum first = EL_SirenMode.MODE_NAMES.Get(0);
			ctx.NotNull(first, "mode entry 0 is present");
			if (first)
				ctx.EqualStr("default", first.m_Key, "the first mode is the default mode");
		}

		// EL_SirenModes.Find resolves a mode by name, so every name is set and unique.
		array<string> seen = {};
		for (int i = 0; i < EL_SirenMode.MODE_NAMES.Count(); i++)
		{
			ParamEnum entry = EL_SirenMode.MODE_NAMES.Get(i);
			ctx.NotNull(entry, string.Format("mode entry %1 is present", i));
			if (!entry)
				continue;

			ctx.False(entry.m_Key.IsEmpty(), string.Format("mode entry %1 has a name", i));
			ctx.False(seen.Contains(entry.m_Key), string.Format("mode name %1 is unique", entry.m_Key));
			seen.Insert(entry.m_Key);
		}
	}
};
