// red-proof: 2026-11-22 `tools/cli.mjs test --no-build --red LF_Test_WorkplaceJobDesk`: RED PROOF OK (RedProof() skips taking the job, so the body holds none and the take assertions fail). No-free-pickaxe assertion: 2026-11-22 island session (next-activities-proof) logged `passed mining step pickaxe (ITEM)` the moment a fresh miner took the job (SetJob gave the old free kit); with SetJob(job, false) it passes.
//! jobs/workplace-job-desk: a gathering-job desk (miner, lumberjack, farmer) offers "Take the job" and the server puts
//! the job on the player, and takes it off again on "Quit the job". Before 2026-11-22 nothing in play signed a player up
//! for MINER, LUMBERJACK or FARMER, so the pickaxe and axe tills never sold. Duty desks and delivery counters keep their
//! own actions: the desk offers nothing for POLICE, MEDIC or PARCEL.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_WorkplaceJobDesk : LF_Test
{
	static const ResourceName WORKPLACE = "{F2DA01057E99744B}Prefabs/Jobs/LF_Workplace.et";
	static const string WORKPLACE_FILE = "$LifeFramework:Prefabs/Jobs/LF_Workplace.et";
	static const ResourceName PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";
	static const string UID = "workplace-job-desk-uid";

	override string GetName()
	{
		return "jobs/workplace-job-desk";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.Equal(LF_EJobType.MINER, LF_WorkplaceJob.DeskJob("MINER"), "a miner desk signs up miners");
		ctx.Equal(LF_EJobType.LUMBERJACK, LF_WorkplaceJob.DeskJob("LUMBERJACK"), "a woodlot desk signs up lumberjacks");
		ctx.Equal(LF_EJobType.FARMER, LF_WorkplaceJob.DeskJob("FARMER"), "an orchard desk signs up farmers");
		ctx.Equal(LF_EJobType.UNEMPLOYED, LF_WorkplaceJob.DeskJob("POLICE"), "a police desk keeps its punch clock");
		ctx.Equal(LF_EJobType.UNEMPLOYED, LF_WorkplaceJob.DeskJob("PARCEL"), "a delivery job keeps its counter");
		ctx.Equal(LF_EJobType.UNEMPLOYED, LF_WorkplaceJob.DeskJob("NOT_A_JOB"), "an id no job has offers nothing");

		// The workplace prefab carries the action.
		Resource resource = Resource.Load(WORKPLACE);
		ctx.True(resource.IsValid(), "the workplace prefab loads");
		ctx.True(PrefabNames(WORKPLACE_FILE, "LF_WorkplaceJobAction"), "the workplace desk offers the take-the-job action");

		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (!body)
			return;

		LF_Utils.SetUidForTests(body, UID);
		LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(body);
		ctx.NotNull(jobs, "the character has a job component");
		if (jobs)
		{
			int taken = -1;
			if (!RedProof())
				taken = LF_WorkplaceJob.Toggle(body, LF_EJobType.MINER);

			ctx.Equal(LF_EJobType.MINER, taken, "the desk signs the player up as a miner");
			ctx.Equal(LF_EJobType.MINER, jobs.GetJob(), "the body holds the miner job");
			ctx.False(LF_TestCharacterFixture.HasItem(body, PICKAXE), "the desk hands out no free pickaxe: it is bought at the miners' till");
			ctx.Equal(LF_EJobType.UNEMPLOYED, LF_WorkplaceJob.Toggle(body, LF_EJobType.MINER), "the same desk lets the miner quit");
			ctx.Equal(LF_EJobType.UNEMPLOYED, jobs.GetJob(), "the body holds no job after quitting");
			ctx.Equal(-1, LF_WorkplaceJob.Toggle(body, LF_EJobType.UNEMPLOYED), "a desk with no gathering job does nothing");
		}

		LF_Utils.SetUidForTests(null, string.Empty);
		LF_TestCharacterFixture.Delete(body);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the prefab file names the class (its desk clock lists the action; a prefab child is no child at runtime,
	//! so the file is read rather than a spawned desk).
	protected bool PrefabNames(string file, string className)
	{
		FileHandle handle = FileIO.OpenFile(file, FileMode.READ);
		if (!handle)
			return false;

		bool found = false;
		string line;
		while (handle.ReadLine(line) >= 0)
		{
			if (line.Contains(className))
			{
				found = true;
				break;
			}
		}

		handle.Close();
		return found;
	}
}
