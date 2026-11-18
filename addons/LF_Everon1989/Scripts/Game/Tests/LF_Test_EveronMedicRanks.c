// red-proof: observed 2026-11-18 `tools\cli test --no-build --red LF_Test_MedicPackOverlay,LF_Test_EveronMedicRanks` printed `RED-OK everon/medic-ranks (fails with the hook on, 5 assertions)`; first failure `the pack ladder has four rungs (expected 4, got 0)`. The hook swaps in an empty header as the pack source. `--only` passes under the pack header.
//! #3047 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the pack's medic overlay
//! gives a four-rung ladder: orderly, EMT, paramedic, doctor. Pay rises with each rung (0.3, 0.4, 0.55, 0.7
//! cash per on-duty second), the first three promote after 1, 2 and 4 hours of duty, the top rung never
//! does, and each rung adds one medicine tier to the kit. Every kit prefab resolves to a real resource and a
//! rank-0 orderly is refused the paramedic's morphine.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronMedicRanks : LF_Test
{
	static const ResourceName OVERLAY = "{6B4D07A19E3C52F8}Configs/Medic/LF_Medic_Everon1989.conf";
	static const ResourceName GAUZE = "{AF3411AAF94D18C2}Prefabs/Items/Medicine/Gauze_02/Gauze_02.et";

	override string GetName()
	{
		return "everon/medic-ranks";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header)
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(OVERLAY) != -1, "the header lists the Everon medic overlay");

		// The act: the pack overlay layered over the core file.
		if (RedProof())
		{
			// No pack layer: an empty header as the pack source.
			LF_MissionConfigPackSource none = new LF_MissionConfigPackSource();
			none.SetHeader(new LF_MissionHeader());
			LF_ConfigService.SetPackSource(none);
		}

		LF_MedicConfig.Reload();
		LF_MedicConfig config = LF_MedicConfig.Get();
		if (RedProof())
		{
			LF_ConfigService.Reset();
			LF_MedicConfig.Reload();
		}

		ctx.NotNull(config, "the medic config loads under the pack header");
		if (!config)
			return;

		ctx.Equal(4, config.m_aRanks.Count(), "the pack ladder has four rungs");
		if (config.m_aRanks.Count() != 4)
			return;

		array<string> ids = {"orderly", "emt", "paramedic", "doctor"};
		array<float> pay = {0.3, 0.4, 0.55, 0.7};
		array<int> seconds = {3600, 7200, 14400, 0};
		foreach (int i, string id : ids)
		{
			ctx.EqualStr(id, config.m_aRanks[i].m_sId, string.Format("rung %1 is %2", i, id));
			ctx.True(Math.AbsFloat(LF_MedicRanks.PayPerSecond(i) - pay[i]) < 0.0001, string.Format("%1 pays %2 per on-duty second", id, pay[i]));
			ctx.Equal(seconds[i], LF_MedicRanks.DutySecondsForNext(i), string.Format("%1 promotes after %2 duty seconds", id, seconds[i]));
			if (i > 0)
				ctx.True(LF_MedicRanks.PayPerSecond(i) > LF_MedicRanks.PayPerSecond(i - 1), string.Format("%1 earns more than the rung below", id));
		}

		ctx.EqualStr("LF-MedicRank_Orderly", LF_MedicRanks.TitleKey(0), "rung 0 uses the pack title");
		ctx.EqualStr("LF-MedicRank_Emt", LF_MedicRanks.TitleKey(1), "rung 1 uses the pack title");
		ctx.EqualStr("LF-MedicRank_Paramedic", LF_MedicRanks.TitleKey(2), "rung 2 reuses the core title");
		ctx.EqualStr("LF-MedicRank_Doctor", LF_MedicRanks.TitleKey(3), "rung 3 reuses the core title");

		// Kit: each rung adds a tier; a higher rung draws everything below it.
		array<ResourceName> kit = {};
		LF_MedicRanks.CollectKit(0, kit);
		ctx.Equal(2, kit.Count(), "an orderly draws the bandage and the tourniquet");
		LF_MedicRanks.CollectKit(1, kit);
		ctx.Equal(3, kit.Count(), "an EMT adds the saline bag");
		LF_MedicRanks.CollectKit(2, kit);
		ctx.Equal(4, kit.Count(), "a paramedic adds the morphine");
		LF_MedicRanks.CollectKit(3, kit);
		ctx.Equal(5, kit.Count(), "a doctor adds the surgical gauze");

		array<ResourceName> everything = {LF_MedicRanks.BANDAGE, LF_MedicRanks.TOURNIQUET, LF_MedicRanks.SALINE, LF_MedicRanks.MORPHINE, GAUZE};
		foreach (ResourceName prefab : everything)
		{
			string name = prefab;
			Resource resource = Resource.Load(prefab);
			ctx.True(resource != null && resource.IsValid(), name + " resolves to a real prefab");
			ctx.True(kit.Contains(prefab), name + " is on the doctor's kit");
		}

		string refusal;
		ctx.True(LF_MedicRanks.CanDrawItem(0, LF_MedicRanks.BANDAGE, refusal), "an orderly draws a bandage");
		ctx.False(LF_MedicRanks.CanDrawItem(0, LF_MedicRanks.MORPHINE, refusal), "an orderly is refused the paramedic's morphine");
		ctx.EqualStr(LF_MedicRanks.KIT_REFUSED_KEY, refusal, "the refusal is put in words");
		ctx.True(LF_MedicRanks.CanDrawItem(2, LF_MedicRanks.MORPHINE, refusal), "a paramedic draws morphine");

		LF_ConfigService.Reset();
		LF_MedicConfig.Reload();
	}
}
