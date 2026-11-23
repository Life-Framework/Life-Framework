// red-proof: pending, see the PR (`test --no-build --red LF_Test_EveronPostalDistances --world "{1054CC8646357708}Missions/LF_PlaytestTown.conf"`).
//! everon/postal-distances (#4326): the playtest town is ~300 x 215 m, so its mission header lists two small overlays that
//! shrink the delivery run's least drop distance (core 300 m to 80 m, LF_JobsConfig) and the postal van run's least
//! distance to the far office (core 800 m to 150 m, LF_PostalRunConfig). Any other header (the island) lists neither and
//! keeps the core numbers. Run the town leg with `test --no-build --only LF_Test_EveronPostalDistances --world
//! "{1054CC8646357708}Missions/LF_PlaytestTown.conf"`; the pack leg proves the island side.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronPostalDistances : LF_Test
{
	static const ResourceName JOBS_OVERLAY = "{ECED7FAD5F5C8B24}Configs/Jobs/LF_Jobs_PlaytestTown.conf";
	static const ResourceName POSTAL_OVERLAY = "{4A8DB06DCD989B6C}Configs/Jobs/LF_PostalRun_PlaytestTown.conf";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/postal-distances";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The overlay files themselves, laid over fresh core values whatever the header says.
		LF_JobsConfig jobs = LF_JobsConfig.Cast(LoadFresh(LF_JobsConfig.CONFIG_PATH));
		LF_PostalRunConfig postal = LF_PostalRunConfig.CreateDefault();
		ctx.NotNull(jobs, "the core jobs config loads");
		ctx.EqualFloat(800, postal.m_fMinDistance, 0.001, "the core far-office distance is 800 m");
		if (jobs)
		{
			ctx.EqualFloat(300, jobs.m_fRunMinDistance, 0.001, "the core drop distance is 300 m");
			Resource jobsLayer = BaseContainerTools.LoadContainer(JOBS_OVERLAY);
			ctx.True(jobsLayer && jobsLayer.GetResource(), "the town jobs overlay resolves");
			if (jobsLayer && jobsLayer.GetResource())
			{
				int changed = jobs.ApplyPackRunDistance(jobsLayer.GetResource().ToBaseContainer(), JOBS_OVERLAY);
				ctx.Equal(1, changed, "the town jobs overlay changes one field");
				ctx.EqualFloat(80, jobs.m_fRunMinDistance, 0.001, "the town overlay sets the drop distance to 80 m");
			}
		}

		Resource postalLayer = BaseContainerTools.LoadContainer(POSTAL_OVERLAY);
		ctx.True(postalLayer && postalLayer.GetResource(), "the town postal overlay resolves");
		if (postalLayer && postalLayer.GetResource())
		{
			int changed = postal.ApplyPackContainer(postalLayer.GetResource().ToBaseContainer(), POSTAL_OVERLAY);
			ctx.Equal(1, changed, "the town postal overlay changes one field");
			ctx.EqualFloat(150, postal.m_fMinDistance, 0.001, "the town overlay sets the far-office distance to 150 m");
		}

		// The running configs under this run's header: the town numbers on the town header, the core's elsewhere.
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the run has a mission header");
		if (!header)
			return;

		bool town = header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(JOBS_OVERLAY) != -1;
		bool townPostal = header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(POSTAL_OVERLAY) != -1;
		ctx.True(town == townPostal, "a header lists both town overlays or neither");
		if (RedProof())
			header.m_aLF_ConfigOverlays = new array<ResourceName>();

		LF_JobsConfig.Reload();
		LF_PostalRunConfig.Reload();
		float wantDrop = 300;
		float wantFar = 800;
		if (town)
		{
			wantDrop = 80;
			wantFar = 150;
		}

		ctx.EqualFloat(wantDrop, LF_CivilianJob.RunMinDistance(), 0.001, "the running drop distance follows the header");
		ctx.EqualFloat(wantFar, LF_PostalRunConfig.Get().m_fMinDistance, 0.001, "the running far-office distance follows the header");

		LF_JobsConfig.Reload();
		LF_PostalRunConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected Managed LoadFresh(ResourceName path)
	{
		Resource core = BaseContainerTools.LoadContainer(path);
		if (!core || !core.GetResource())
			return null;

		return BaseContainerTools.CreateInstanceFromContainer(core.GetResource().ToBaseContainer());
	}
}
