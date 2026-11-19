// red-proof: before the three prefabs exist (or with `m_fSecondsOff` raised to 20 on each), `tools\cli test --no-build --only LF_Test_EveronJailWork` fails `jail work prefab loads` and `a full round of the three stays within half of the shortest sentence`. RedProof() raises the laundry tub's seconds-off in the read copy, so the round passes the half and the check fails.
//! #3418: the 1989 jail's three work stations (laundry tub, kitchen sink, yard broom) each carry the existing
//! jail work action (#127, LF_JailWork) with seconds-off, work time and cooldown inside the card's bounds, each
//! its own job id, and a full round of the three never takes more than half of the shortest sentence off.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronJailWork : LF_Test
{
	protected static const ResourceName LAUNDRY = "{73912A59FDEEF731}Prefabs/Props/Jail/LF_JailLaundryTub.et";
	protected static const ResourceName KITCHEN = "{AD8A93936711F255}Prefabs/Props/Jail/LF_JailKitchenSink.et";
	protected static const ResourceName YARD = "{4DFF61251337EEF8}Prefabs/Props/Jail/LF_JailYardBroom.et";
	protected static const ResourceName CELL = "{AA32D528D592E54C}Prefabs/Police/EL_JailCell.et";

	protected static const float OFF_MAX = 15;	// lf-fixed: card bound, seconds off per task
	protected static const float WORK_MIN = 3;	// lf-fixed: card bound, shortest work time
	protected static const float WORK_MAX = 10;	// lf-fixed: card bound, longest work time
	protected static const float COOLDOWN_MIN = 20;	// lf-fixed: card bound, shortest cooldown

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/jail-work";
	}

	//------------------------------------------------------------------------------------------------
	//! Finds the prefab's LF_JailWorkAction container; null when there is none (the ctx gets a failure).
	protected BaseContainer FindAction(LF_TestContext ctx, ResourceName prefab, string label)
	{
		Resource res = Resource.Load(prefab);
		ctx.True(res && res.IsValid(), label + ": jail work prefab loads");
		if (!res || !res.IsValid())
			return null;

		IEntitySource source = res.GetResource().ToEntitySource();
		if (!source)
			return null;

		BaseContainer found;
		int count = 0;
		for (int i = 0, n = source.GetComponentCount(); i < n; i++)
		{
			IEntityComponentSource component = source.GetComponent(i);
			if (component.GetClassName() != "ActionsManagerComponent")
				continue;

			BaseContainerList actions = component.GetObjectArray("additionalActions");
			if (!actions)
				continue;

			for (int a = 0; a < actions.Count(); a++)
			{
				BaseContainer action = actions.Get(a);
				if (action.GetClassName() != "LF_JailWorkAction")
					continue;

				found = action;
				count++;
			}
		}

		ctx.Equal(1, count, label + ": carries exactly one jail work action");
		return found;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ResourceName> prefabs = {LAUNDRY, KITCHEN, YARD};
		array<string> labels = {"laundry", "kitchen", "yard"};
		array<string> jobs = {};
		array<float> offs = {};
		array<float> cooldowns = {};
		float total = 0;

		for (int i = 0; i < prefabs.Count(); i++)
		{
			string label = labels[i];
			BaseContainer action = FindAction(ctx, prefabs[i], label);
			if (!action)
				continue;

			string job;
			float off;
			float cooldown;
			float work;
			action.Get("m_sJobId", job);
			action.Get("m_fSecondsOff", off);
			action.Get("m_fCooldownSeconds", cooldown);
			action.Get("m_fWorkSeconds", work);

			if (RedProof() && i == 0)
				off = 20;

			ctx.True(off > 0 && off <= OFF_MAX, string.Format("%1: seconds off is within 1..%2 (got %3)", label, OFF_MAX, off));
			ctx.True(work >= WORK_MIN && work <= WORK_MAX, string.Format("%1: work time is within %2..%3 s (got %4)", label, WORK_MIN, WORK_MAX, work));
			ctx.True(cooldown >= COOLDOWN_MIN, string.Format("%1: cooldown is at least %2 s (got %3)", label, COOLDOWN_MIN, cooldown));
			ctx.False(job.IsEmpty(), label + ": has a job id");
			ctx.True(jobs.Find(job) < 0, label + ": its job id is not shared with another station (the cooldown is per job)");

			jobs.Insert(job);
			offs.Insert(off);
			cooldowns.Insert(cooldown);
			total += off;
		}

		if (jobs.Count() != prefabs.Count())
			return;

		int shortest = ShortestSentence(ctx);
		ctx.True(shortest > 0, "the cell prefab gives a shortest sentence");
		ctx.True(total <= shortest / 2, string.Format("a full round of the three stays within half of the shortest sentence (%1 s off of %2 s)", total, shortest));

		RunRound(ctx, jobs, offs, cooldowns, shortest, total);
	}

	//------------------------------------------------------------------------------------------------
	//! One wanted level on the core cell prefab: the shortest sentence an arrest hands out.
	protected int ShortestSentence(LF_TestContext ctx)
	{
		Resource res = Resource.Load(CELL);
		if (!res || !res.IsValid())
			return 0;

		IEntitySource source = res.GetResource().ToEntitySource();
		if (!source)
			return 0;

		for (int i = 0, n = source.GetComponentCount(); i < n; i++)
		{
			IEntityComponentSource component = source.GetComponent(i);
			if (component.GetClassName() != "LF_JailCellComponent")
				continue;

			int perWanted;
			component.Get("m_iSecondsPerWanted", perWanted);
			return perWanted;
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! A prisoner works all three once: the sentence drops by the sum and stays above half.
	protected void RunRound(LF_TestContext ctx, array<string> jobs, array<float> offs, array<float> cooldowns, int shortest, float total)
	{
		string officerUid = "everon-jailwork-officer";
		string prisonerUid = "everon-jailwork-prisoner";

		LF_Jail.Configure(shortest, 300, 1, 5);
		LF_JailWork.ResetStatics();

		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		LF_PlayerAccount prisoner = LF_PlayerAccountManager.GetOrCreate(prisonerUid);
		prisoner.SetWantedLevel(1);

		ctx.True(LF_Jail.Arrest(officerUid, prisonerUid, null), "the arrest starts the shortest sentence");
		ctx.EqualFloat(shortest, LF_Jail.GetRemaining(prisonerUid), 0.01, "the shortest sentence is one wanted level");

		for (int i = 0; i < jobs.Count(); i++)
			ctx.Equal(LF_EJailWorkCheck.OK, LF_JailWork.Complete(prisonerUid, jobs[i], offs[i], cooldowns[i], 100 + i), "the " + jobs[i] + " task is accepted");

		ctx.EqualFloat(shortest - total, LF_Jail.GetRemaining(prisonerUid), 0.01, "a full round takes exactly the three configured amounts off");
		ctx.True(LF_Jail.IsServing(prisonerUid), "a round of the three does not end the shortest sentence");

		LF_JailWork.ResetStatics();
		LF_Jail.SyncPlacedCell();
	}
}
