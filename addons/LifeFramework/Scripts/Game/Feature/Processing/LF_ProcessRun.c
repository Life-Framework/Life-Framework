//------------------------------------------------------------------------------------------------
//! One timed run at a machine (#3229): the outputs a worker is owed, where they drop, and when.
//! Session-only: a run is never persisted. A world end hands the outputs over at once.
class LF_ProcessRunJob
{
	string m_sKey;
	string m_sMachineId;
	EntityID m_Machine;
	EntityID m_User;
	vector m_vDrop;
	vector m_vRotation;
	int m_iBatches;
	float m_fDue;
	ref array<ref LF_ProcessingOutput> m_aOutputs = {};
}

//------------------------------------------------------------------------------------------------
//! Machines that take time (#3229). A machine row with `m_iProcessSeconds` > 0 takes the inputs when
//! a worker starts it; the outputs drop at the machine once the time is up. One run at a time per
//! machine entity: a second start is refused as busy. The machine is heard running for the run.
//!
//! The run state is session-only. The inputs left the inventory at the start, so a world end
//! (the game mode's OnGameEnd, before the engine's shutdown save) drops every owed output at its
//! machine at once and loses nothing. Server only; a client holds no runs.
class LF_ProcessRun
{
	static const string FEATURE = "Processing";

	protected static ref map<string, ref LF_ProcessRunJob> s_mJobs;
	protected static ref LF_ProcessRunWorldEnd s_Listener;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_ProcessRunJob> Jobs()
	{
		if (!s_mJobs)
			s_mJobs = new map<string, ref LF_ProcessRunJob>();

		return s_mJobs;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds on the world clock; what Start and CompleteDue read when no time is given.
	static float Now()
	{
		return GetGame().GetWorld().GetWorldTime() / 1000;
	}

	//------------------------------------------------------------------------------------------------
	protected static string KeyOf(IEntity machine)
	{
		if (!machine)
			return string.Empty;

		return machine.GetID().ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! True while a run is under way at \p machine (until its outputs have dropped).
	static bool IsBusy(IEntity machine)
	{
		string key = KeyOf(machine);
		if (key.IsEmpty() || !s_mJobs)
			return false;

		return s_mJobs.Contains(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Runs owed right now, across every machine.
	static int Count()
	{
		if (!s_mJobs)
			return 0;

		return s_mJobs.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: starts a run at \p machine. The caller has taken the inputs already; \p outputs (whole
	//! batches already scaled) are owed at the machine's position plus \p offset after \p seconds.
	//! \return False, and nothing is kept, when the machine is busy or the arguments are no run.
	static bool Start(IEntity machine, IEntity user, string machineId, array<ref LF_ProcessingOutput> outputs, int batches, float seconds, vector offset, vector rotation)
	{
		if (!Replication.IsServer() || !machine || !outputs || outputs.IsEmpty() || seconds <= 0)
			return false;

		if (IsBusy(machine))
			return false;

		LF_ProcessRunJob job = new LF_ProcessRunJob();
		job.m_sKey = KeyOf(machine);
		job.m_sMachineId = machineId;
		job.m_Machine = machine.GetID();
		if (user)
			job.m_User = user.GetID();
		job.m_vDrop = machine.GetOrigin() + offset;
		job.m_vRotation = rotation;
		job.m_iBatches = batches;
		job.m_fDue = Now() + seconds;
		foreach (LF_ProcessingOutput output : outputs)
		{
			if (!output)
				continue;

			LF_ProcessingOutput row = new LF_ProcessingOutput();
			row.m_OutputPrefab = output.m_OutputPrefab;
			row.m_iOutputAmount = output.m_iOutputAmount;
			job.m_aOutputs.Insert(row);
		}

		Jobs().Set(job.m_sKey, job);
		EnsureWorldEndListener();
		GetGame().GetCallqueue().CallLater(OnTimer, seconds * 1000, false, job.m_sKey);
		// Heard running for the whole run (Feature/Atmosphere); a machine without the switch stays quiet.
		LF_AmbientSwitchComponent.PulseOn(machine, seconds);
		// Seen running too: fire glow, smoke and the time left on its row (LF_ProcessRunFxComponent).
		LF_ProcessRunFxComponent.RunStarted(machine, seconds);
		LF_Debug.Info(FEATURE, string.Format("run started at %1 (%2): %3 batch(es), outputs in %4 s", machineId, job.m_sKey, batches, seconds));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void OnTimer(string key)
	{
		Deliver(key, "time is up");
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the outputs of every run whose time is up at \p now.
	//! \return The runs delivered.
	static int CompleteDue(float now)
	{
		if (!s_mJobs)
			return 0;

		array<string> due = {};
		foreach (string key, LF_ProcessRunJob job : s_mJobs)
		{
			if (job.m_fDue <= now)
				due.Insert(key);
		}

		foreach (string dueKey : due)
			Deliver(dueKey, "time is up");

		return due.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the outputs of every run now, whatever its time. The world end calls this before the save.
	//! \return The runs delivered.
	static int CompleteAll(string why)
	{
		if (!s_mJobs)
			return 0;

		array<string> keys = {};
		foreach (string key, LF_ProcessRunJob job : s_mJobs)
			keys.Insert(key);

		foreach (string owed : keys)
			Deliver(owed, why);

		return keys.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Hands one run's outputs over at the machine and ends the run. The run leaves the table first,
	//! so a second call (a late timer after CompleteDue) finds nothing and drops nothing twice.
	protected static void Deliver(string key, string why)
	{
		if (!s_mJobs)
			return;

		LF_ProcessRunJob job;
		if (!s_mJobs.Find(key, job) || !job)
			return;

		s_mJobs.Remove(key);
		LF_ProcessRunFxComponent.RunEnded(GetGame().GetWorld().FindEntityByID(job.m_Machine));

		int dropped;
		foreach (LF_ProcessingOutput output : job.m_aOutputs)
		{
			for (int i = 0; i < output.m_iOutputAmount; i++)
			{
				IEntity made = LF_Utils.SpawnEntityPrefab(output.m_OutputPrefab, job.m_vDrop, job.m_vRotation);
				if (made)
					dropped++;
				else
					LF_Debug.Error(FEATURE, string.Format("run at %1: an output of %2 did not spawn", job.m_sMachineId, output.m_OutputPrefab));
			}

			LF_Debug.Log(FEATURE, string.Format("processed %1 x%2 (%3 batch(es), timed)", output.m_OutputPrefab, output.m_iOutputAmount, job.m_iBatches));
		}

		LF_Debug.Info(FEATURE, string.Format("run at %1 done (%2): %3 output(s) dropped", job.m_sMachineId, why, dropped));

		// The worker is paid for the batches when the parts come out, if they are still in the world.
		IEntity user = GetGame().GetWorld().FindEntityByID(job.m_User);
		if (!user)
			return;

		LF_JobManager jobManager = LF_JobManager.GetInstance();
		if (jobManager)
		{
			foreach (LF_ProcessingOutput paid : job.m_aOutputs)
			{
				for (int batch = 0; batch < job.m_iBatches; batch++)
					jobManager.OnProcessCompleted(user, paid.m_OutputPrefab);
			}
		}

		LF_Utils.NotifyEntity(user, LF_ProcessAction.ToastText(job.m_aOutputs), "", 4.0);
	}

	//------------------------------------------------------------------------------------------------
	protected static void EnsureWorldEndListener()
	{
		if (s_Listener)
			return;

		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (!gameMode)
		{
			LF_Debug.Warn(FEATURE, "no game mode: a world end will not hand the machine outputs over");
			return;
		}

		s_Listener = new LF_ProcessRunWorldEnd();
		gameMode.GetOnGameEnd().Insert(s_Listener.OnGameEnd);
	}

	//------------------------------------------------------------------------------------------------
	//! World end: the timers and the table go (LF_StaticReset). The game mode is already ending, so a
	//! run left here is only counted; OnGameEnd handed the outputs over before the save.
	static void ResetStatics()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(OnTimer);

		if (s_mJobs && !s_mJobs.IsEmpty())
			LF_Debug.Warn(FEATURE, string.Format("world end: %1 timed run(s) were still owed and are dropped", s_mJobs.Count()));

		SCR_BaseGameMode gameMode;
		if (GetGame())
			gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gameMode && s_Listener)
			gameMode.GetOnGameEnd().Remove(s_Listener.OnGameEnd);

		s_Listener = null;
		s_mJobs = null;
	}
}

//------------------------------------------------------------------------------------------------
//! The game mode's end-of-session callback for LF_ProcessRun; an invoker needs an instance method.
class LF_ProcessRunWorldEnd : Managed
{
	//------------------------------------------------------------------------------------------------
	void OnGameEnd()
	{
		if (!Replication.IsServer())
			return;

		int done = LF_ProcessRun.CompleteAll("session end");
		if (done > 0)
			LF_Debug.Info(LF_ProcessRun.FEATURE, string.Format("session end: %1 timed run(s) handed over before the save", done));
	}
}

//------------------------------------------------------------------------------------------------
class LF_ProcessRun_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ProcessRun";
	}

	override void Apply()
	{
		LF_ProcessRun.ResetStatics();
	}
}
