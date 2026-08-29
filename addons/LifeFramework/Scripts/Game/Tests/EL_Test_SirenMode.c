// red-proof: remove one entry from EL_SirenMode.MODE_NAMES (or change N_MODES) and run
// `tools\cli test --tier fast`; the count/name assertions fail because the arrays
// disagree.

// tier: LOGIC
class EL_Test_SirenMode : EL_Test
{
	override string GetName()
	{
		return "siren/mode-enum-integrity";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.True(EL_SirenMode.N_MODES > 0, "siren mode count is positive");
		ctx.Equal(EL_SirenMode.N_MODES, EL_SirenMode.MODE_NAMES.Count(), "N_MODES matches MODE_NAMES length");

		ctx.True(EL_SirenMode.SIREN_SOUNDS.Count() > 0, "siren sound list is not empty");

		array<string> names = {"default", "Mode1", "Mode2", "Mode3", "Mode4", "Mode5", "Mode6", "Mode7"};
		ctx.Equal(names.Count(), EL_SirenMode.N_MODES, "default mode list length matches N_MODES");

		for (int i = 0; i < EL_SirenMode.MODE_NAMES.Count(); i++)
		{
			ParamEnum entry = EL_SirenMode.MODE_NAMES.Get(i);
			ctx.NotNull(entry, string.Format("mode entry %1 is present", i));
			if (entry)
				ctx.False(entry.m_Key.IsEmpty(), string.Format("mode entry %1 has a name", i));
		}
	}
};