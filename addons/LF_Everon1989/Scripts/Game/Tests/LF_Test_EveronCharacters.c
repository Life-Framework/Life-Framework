// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_CharacterSwitchRules,LF_Test_CharacterIsolation,LF_Test_EveronCharacters`: RED-OK, with the hook reading the core file without its pack layers it failed `- Everon: an account holds 3 characters (expected 3, got 1)`; green without it (pack leg).
//! Under the Everon 1989 header (the pack leg of `tools\cli test`) an account holds 3 characters: the
//! header lists the pack's LF_CharacterConfig overlay and the running config reads it over the core
//! file, which ships 1. The town clerk offers the character switch there.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronCharacters : LF_Test
{
	static const ResourceName OVERLAY = "{C617C9A0836A0622}Configs/Account/LF_Characters_Everon1989.conf";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/characters-per-account";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header)
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(OVERLAY) != -1, "the header lists the Everon character overlay");

		LF_CharacterConfig.SetMaxPerAccountOverride(0);
		LF_CharacterConfig config;
		if (!RedProof())
		{
			LF_CharacterConfig.Reload();
			config = LF_CharacterConfig.Get();
		}
		else
		{
			Resource core = BaseContainerTools.LoadContainer(LF_CharacterConfig.CONFIG_PATH);
			if (core && core.GetResource())
				config = LF_CharacterConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(core.GetResource().ToBaseContainer()));
		}

		ctx.NotNull(config, "the character config loads");
		if (config)
			ctx.Equal(3, config.m_iMaxPerAccount, "Everon: an account holds 3 characters");

		ctx.True(LF_DialogueHook.Find("CharacterSwitchOffered") != null, "the clerk's switch condition hook exists");
		ctx.True(LF_DialogueHook.Find("CharacterSwitch") != null, "the clerk's switch action hook exists");
	}
}
