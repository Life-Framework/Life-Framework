// red-proof: expected 25 job XP after AddExperience and ran `tools\cli test --no-build --tier fast`
// on 2026-10-11. The suite reported
// `[ELTEST] - AddExperience adds job XP (expected 25, got 0)`.
// AddExperience was a no-op. The grant now adds XP and levels the job.

// tier: LOGIC
class EL_Test_Roadmap_Jobs : EL_Test
{
	override string GetName()
	{
		return "roadmap/jobs";
	}

	override void Run(EL_TestContext ctx)
	{
		string xpUid = "roadmap-jobs-xp";
		LF_CivilianJob.AddExperience(xpUid, 25);
		ctx.EqualFloat(25, LF_CivilianJob.GetExperience(xpUid), 0.01, "AddExperience adds job XP");

		string uid = "roadmap-jobs-logic";
		EL_ATMManager atm = EL_ATMManager.GetInstance();
		ctx.NotNull(atm, "ATM manager exists");
		if (ctx.FailureCount() > 0)
			return;

		atm.CreateAccount(uid);
		EL_BankAccount bank = atm.GetAccount(uid);
		ctx.NotNull(bank, "runner bank account exists");
		if (bank)
			ctx.Equal(0, bank.GetBalance(), "bank starts at 0");

		ctx.False(LF_CivilianJob.SignUp(uid, EL_EJobType.FUEL_DELIVERY), "under-level fuel signup refuses");
		ctx.False(LF_CivilianJob.SignUp(uid, EL_EJobType.TAXI), "under-level taxi signup refuses");
		ctx.True(LF_CivilianJob.SignUp(uid, EL_EJobType.PARCEL), "level 1 can take courier work");
		ctx.True(LF_CivilianJob.StartRun(uid, EL_EJobType.PARCEL), "parcel run starts");

		int paid = LF_CivilianJob.CompleteRun(uid);
		ctx.Equal(LF_CivilianJob.PAY_PARCEL, paid, "a completed run pays once");
		ctx.Equal(LF_CivilianJob.PAY_PARCEL, LF_CivilianJob.GetPaidTotal(uid), "paid total is the one fare");
		ctx.Equal(2, LF_CivilianJob.GetLevel(uid), "a completed run levels the job");
		ctx.Equal(0, LF_CivilianJob.CompleteRun(uid), "the same run does not pay again");
		ctx.Equal(LF_CivilianJob.PAY_PARCEL, LF_CivilianJob.GetPaidTotal(uid), "a second complete adds nothing");

		ctx.True(LF_CivilianJob.SignUp(uid, EL_EJobType.FUEL_DELIVERY), "level 2 opens fuel delivery");
		ctx.False(LF_CivilianJob.SignUp(uid, EL_EJobType.TAXI), "level 2 taxi signup refuses");

		ctx.True(LF_CivilianJob.StartRun(uid, EL_EJobType.FUEL_DELIVERY), "fuel run starts");
		ctx.True(LF_CivilianJob.AbandonRun(uid), "an active run can be abandoned");
		ctx.Equal(LF_CivilianJob.PAY_PARCEL, LF_CivilianJob.GetPaidTotal(uid), "an abandoned run pays nothing");
		ctx.Equal(0, LF_CivilianJob.CompleteRun(uid), "an abandoned run cannot be completed");

		ctx.True(LF_CivilianJob.StartRun(uid, EL_EJobType.FUEL_DELIVERY), "a new fuel run starts after abandon");
		ctx.Equal(LF_CivilianJob.PAY_FUEL, LF_CivilianJob.CompleteRun(uid), "the new run pays once");
		ctx.Equal(LF_CivilianJob.PAY_PARCEL + LF_CivilianJob.PAY_FUEL, LF_CivilianJob.GetPaidTotal(uid), "each run pays once");
		ctx.Equal(3, LF_CivilianJob.GetLevel(uid), "the fuel run levels the job again");
		ctx.True(LF_CivilianJob.SignUp(uid, EL_EJobType.TAXI), "level 3 opens taxi");
		ctx.True(LF_CivilianJob.StartRun(uid, EL_EJobType.TAXI), "taxi fare starts without a second client");
		ctx.Equal(LF_CivilianJob.PAY_TAXI, LF_CivilianJob.CompleteRun(uid), "taxi fare pays the fixed amount");

		bank = atm.GetAccount(uid);
		if (bank)
			ctx.Equal(0, bank.GetBalance(), "runs pay cash, the bank stays at 0");
	}
}

// tier: WORLD
class EL_Test_Roadmap_JobsWorld : EL_Test
{
	override string GetName()
	{
		return "e2e/roadmap-jobs";
	}

	override void Run(EL_TestContext ctx)
	{
		Resource parcel = Resource.Load(LF_CivilianJob.PARCEL_PREFAB);
		ctx.True(parcel.IsValid(), "hand-carry parcel prefab loads");
		Resource fuel = Resource.Load(LF_CivilianJob.FUEL_PREFAB);
		ctx.True(fuel.IsValid(), "fuel can prefab loads");
		Resource taxiPrefab = Resource.Load(LF_CivilianJob.TAXI_PREFAB);
		ctx.True(taxiPrefab.IsValid(), "taxi S105 prefab loads");
		if (ctx.FailureCount() > 0)
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		IEntity crate = GetGame().SpawnEntityPrefab(parcel, GetGame().GetWorld(), params);
		ctx.True(crate != null, "parcel crate spawns");
		if (crate)
		{
			EL_HandCarryComponent carry = EL_Component<EL_HandCarryComponent>.Find(crate);
			ctx.NotNull(carry, "parcel crate is the hand-carry prefab");
		}

		IEntity depot = GetGame().GetWorld().FindEntityByName("Roadmap_Job_Depot");
		IEntity drop = GetGame().GetWorld().FindEntityByName("Roadmap_Job_Drop");
		IEntity taxi = GetGame().GetWorld().FindEntityByName("Roadmap_Job_TaxiPad");
		IEntity fuelCan = GetGame().GetWorld().FindEntityByName("Roadmap_Job_FuelCan");
		ctx.True(depot != null, "Roadmap_Job_Depot is placed");
		ctx.True(drop != null, "Roadmap_Job_Drop is placed");
		ctx.True(taxi != null, "Roadmap_Job_TaxiPad is placed");
		ctx.True(fuelCan != null, "Roadmap_Job_FuelCan is placed");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(CountSignups(depot, EL_EJobType.PARCEL) == 1, "depot lists parcel work");
		ctx.True(CountSignups(depot, EL_EJobType.FUEL_DELIVERY) == 1, "depot lists fuel delivery");
		ctx.True(CountSignups(depot, EL_EJobType.TAXI) == 1, "depot lists taxi work");
		ctx.True(HasStart(depot, EL_EJobType.PARCEL), "depot picks up the parcel");
		ctx.True(HasStart(fuelCan, EL_EJobType.FUEL_DELIVERY), "fuel can starts a fuel run");
		ctx.True(HasStart(taxi, EL_EJobType.TAXI), "taxi pad starts a fare");
		ctx.True(HasDropoff(drop), "drop completes the run");

		string uid = "roadmap-jobs-e2e";
		EL_ATMManager atm = EL_ATMManager.GetInstance();
		if (atm)
			atm.CreateAccount(uid);

		ctx.True(LF_CivilianJob.SignUp(uid, EL_EJobType.PARCEL), "e2e parcel signup");
		ctx.True(LF_CivilianJob.StartRun(uid, EL_EJobType.PARCEL), "parcel leaves the depot");
		ctx.Equal(LF_CivilianJob.PAY_PARCEL, LF_CivilianJob.CompleteRun(uid), "parcel drop pays");

		ctx.True(LF_CivilianJob.SignUp(uid, EL_EJobType.FUEL_DELIVERY), "e2e fuel signup after the parcel run");
		ctx.True(LF_CivilianJob.StartRun(uid, EL_EJobType.FUEL_DELIVERY), "fuel run starts from the can");
		ctx.Equal(LF_CivilianJob.PAY_FUEL, LF_CivilianJob.CompleteRun(uid), "fuel delivery pays");

		ctx.True(LF_CivilianJob.SignUp(uid, EL_EJobType.TAXI), "e2e taxi signup");
		ctx.True(LF_CivilianJob.StartRun(uid, EL_EJobType.TAXI), "taxi fare starts");
		ctx.Equal(LF_CivilianJob.PAY_TAXI, LF_CivilianJob.CompleteRun(uid), "taxi fare pays the fixed amount without a second client");

		if (atm)
		{
			EL_BankAccount bank = atm.GetAccount(uid);
			if (bank)
				ctx.Equal(0, bank.GetBalance(), "world runs leave the bank at 0");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected int CountSignups(IEntity entity, EL_EJobType job)
	{
		int count = 0;
		array<BaseUserAction> actionList = {};
		ActionsManagerComponent actions = EL_Component<ActionsManagerComponent>.Find(entity);
		if (!actions)
			return 0;

		actions.GetActionsList(actionList);
		foreach (BaseUserAction action : actionList)
		{
			LF_JobSignUpAction signup = LF_JobSignUpAction.Cast(action);
			if (signup && signup.GetJob() == job)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasStart(IEntity entity, EL_EJobType job)
	{
		array<BaseUserAction> actionList = {};
		ActionsManagerComponent actions = EL_Component<ActionsManagerComponent>.Find(entity);
		if (!actions)
			return false;

		actions.GetActionsList(actionList);
		foreach (BaseUserAction action : actionList)
		{
			LF_JobStartRunAction start = LF_JobStartRunAction.Cast(action);
			if (start && start.GetJob() == job)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasDropoff(IEntity entity)
	{
		array<BaseUserAction> actionList = {};
		ActionsManagerComponent actions = EL_Component<ActionsManagerComponent>.Find(entity);
		if (!actions)
			return false;

		actions.GetActionsList(actionList);
		foreach (BaseUserAction action : actionList)
		{
			if (LF_JobDropoffAction.Cast(action))
				return true;
		}

		return false;
	}
}
