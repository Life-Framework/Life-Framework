// red-proof: with `RedProof()` on the alias table is not asked, so no saved crop, chain, job or station id is rewritten and
// every "renamed" check fails.

//! persist/save-ids-crop-job-rows: renamed crops, chains and jobs still load their saved progress (LF_SaveIds). A farm plot
//! rewrites its crop and keeps its growth seconds, a chain station rewrites its station place and its chain and keeps the step
//! and the work done, a job XP row and a playtime row rewrite the job and keep the XP and seconds. An id nothing renames loads
//! as itself, each kind only follows its own aliases, and a null list or row never stops a load.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_SaveIdsCropJobRows : LF_Test
{
	override string GetName()
	{
		return "persist/save-ids-crop-job-rows";
	}

	override void Run(LF_TestContext ctx)
	{
		string cropNew = "-";
		string cropKept = "-";
		int cropGrown = -1;
		string plotKey = "-";
		string stationNew = "-";
		string chainNew = "-";
		string stepKept = "-";
		int workKept = -1;
		int neededKept = -1;
		string chainPlaceMixup = "-";
		string xpJobNew = "-";
		int xpKept = -1;
		string xpUid = "-";
		string xpJobKept = "-";
		string playJobNew = "-";
		int playSeconds = -1;
		string playUid = "-";
		string cropOfJobText = "-";
		bool survived = false;
		if (!RedProof())
		{
			LF_SaveIds.ResetStatics();
			LF_SaveIds.Add(LF_SaveIds.KIND_CROP, "old_crop", "new_crop");
			LF_SaveIds.Add(LF_SaveIds.KIND_CHAIN, "old_chain", "new_chain");
			LF_SaveIds.Add(LF_SaveIds.KIND_PLACE, "old_station", "new_station");
			LF_SaveIds.Add(LF_SaveIds.KIND_JOB, "OLD_JOB", "NEW_JOB");

			array<ref LF_FarmPlotRow> plots = {};
			LF_FarmPlotRow grown = new LF_FarmPlotRow();
			grown.m_sPlotKey = "plot-a";
			grown.m_sCropId = "old_crop";
			grown.m_iGrownSeconds = 321;
			plots.Insert(grown);
			LF_FarmPlotRow other = new LF_FarmPlotRow();
			other.m_sPlotKey = "plot-b";
			other.m_sCropId = "tomato";
			plots.Insert(other);
			plots.Insert(null);
			LF_SaveIds.ResolveFarmPlots(plots);
			cropNew = grown.m_sCropId;
			cropGrown = grown.m_iGrownSeconds;
			plotKey = grown.m_sPlotKey;
			cropKept = other.m_sCropId;
			LF_SaveIds.ResolveFarmPlots(null);

			array<ref LF_ChainStationRow> stations = {};
			LF_ChainStationRow station = new LF_ChainStationRow();
			station.m_sStationKey = "old_station";
			station.m_sChainId = "old_chain";
			station.m_sStepId = "old_chain";
			station.m_iWork = 4;
			station.m_iNeeded = 9;
			stations.Insert(station);
			stations.Insert(null);
			LF_SaveIds.ResolveChainStations(stations);
			stationNew = station.m_sStationKey;
			chainNew = station.m_sChainId;
			stepKept = station.m_sStepId;
			workKept = station.m_iWork;
			neededKept = station.m_iNeeded;
			LF_SaveIds.ResolveChainStations(null);

			// A station key that matches a chain alias's text is a place, not a chain.
			array<ref LF_ChainStationRow> mixed = {};
			LF_ChainStationRow mix = new LF_ChainStationRow();
			mix.m_sStationKey = "old_chain";
			mix.m_sChainId = "old_station";
			mixed.Insert(mix);
			LF_SaveIds.ResolveChainStations(mixed);
			chainPlaceMixup = mix.m_sStationKey + "|" + mix.m_sChainId;

			array<ref LF_JobXpRow> xp = {};
			LF_JobXpRow xpRow = LF_JobXpRow.Create("uid-1", "OLD_JOB", 4500);
			xp.Insert(xpRow);
			LF_JobXpRow xpOther = LF_JobXpRow.Create("uid-1", "FARMER", 10);
			xp.Insert(xpOther);
			xp.Insert(null);
			LF_SaveIds.ResolveJobXp(xp);
			xpJobNew = xpRow.m_sJob;
			xpKept = xpRow.m_iXp;
			xpUid = xpRow.m_sPlayerUid;
			xpJobKept = xpOther.m_sJob;
			LF_SaveIds.ResolveJobXp(null);

			array<ref LF_PlaytimeRecord> playtime = {};
			LF_PlaytimeRecord playRow = LF_PlaytimeRecord.Create("uid-2", "OLD_JOB", 7200);
			playtime.Insert(playRow);
			playtime.Insert(null);
			LF_SaveIds.ResolvePlaytime(playtime);
			playJobNew = playRow.m_sJob;
			playSeconds = playRow.m_iSeconds;
			playUid = playRow.m_sAccountUid;
			LF_SaveIds.ResolvePlaytime(null);

			// A job alias does not rename a crop with the same text.
			array<ref LF_FarmPlotRow> jobText = {};
			LF_FarmPlotRow jobTextRow = new LF_FarmPlotRow();
			jobTextRow.m_sCropId = "OLD_JOB";
			jobText.Insert(jobTextRow);
			LF_SaveIds.ResolveFarmPlots(jobText);
			cropOfJobText = jobTextRow.m_sCropId;

			survived = true;
			LF_SaveIds.ResetStatics();
		}

		ctx.EqualStr("new_crop", cropNew, "a renamed crop loads under its new id");
		ctx.Equal(321, cropGrown, "the plot keeps the growth it had");
		ctx.EqualStr("plot-a", plotKey, "the plot key is a placed object's key here and is left alone when no place alias names it");
		ctx.EqualStr("tomato", cropKept, "a crop nothing renamed loads as itself");
		ctx.EqualStr("new_station", stationNew, "a chain station follows a renamed place");
		ctx.EqualStr("new_chain", chainNew, "a chain station follows a renamed chain");
		ctx.EqualStr("old_chain", stepKept, "the step id is not a chain id and is never aliased");
		ctx.Equal(4, workKept, "the work done at the station stays as saved");
		ctx.Equal(9, neededKept, "the work needed stays as saved");
		ctx.EqualStr("old_chain|old_station", chainPlaceMixup, "a place alias renames only a station key and a chain alias only a chain id: text of the other kind stays");
		ctx.EqualStr("NEW_JOB", xpJobNew, "a renamed job keeps its saved XP under the new job");
		ctx.Equal(4500, xpKept, "the saved XP is untouched");
		ctx.EqualStr("uid-1", xpUid, "the XP row still belongs to the same player");
		ctx.EqualStr("FARMER", xpJobKept, "a job nothing renamed loads as itself");
		ctx.EqualStr("NEW_JOB", playJobNew, "a renamed job keeps its playtime under the new job");
		ctx.Equal(7200, playSeconds, "the saved playtime is untouched");
		ctx.EqualStr("uid-2", playUid, "the playtime row still belongs to the same account");
		ctx.EqualStr("OLD_JOB", cropOfJobText, "a job alias does not rename a crop of the same text");
		ctx.True(survived, "a null list and a null row never stop a load");
	}
}
