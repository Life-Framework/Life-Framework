// LF_QuestHooksProgress.c - SIGNAL keys for what a player does after the arrival chain (#3123):
// buying a car or a plot, collecting the firearm permit, selling a fish, reaching a job level.
// Each is a modded-class override on the feature's own success path, so the vehicle, property,
// licence, shop and level files stay untouched. A refused purchase or sale reports nothing.

//------------------------------------------------------------------------------------------------
//! SIGNAL keys for the hour-2 milestones and the server helpers behind them.
class LF_QuestProgressSignals
{
	static const string VEHICLE_BOUGHT = "vehicle_bought";
	static const string PLOT_BOUGHT = "plot_bought";
	static const string PERMIT_COLLECTED = "permit_collected";
	static const string FISH_SOLD = "fish_sold";
	static const string JOB_LEVEL = "job_level:";

	//! Test seam: a body no player controls, by the uid the level event carries.
	protected static ref map<string, EntityID> s_mTestBodies;

	//------------------------------------------------------------------------------------------------
	//! "job_level:<JOB>:<n>", the job id as the jobs config spells it ("DELIVERY") and the level reached.
	static string JobLevel(string job, int level)
	{
		return JOB_LEVEL + job + ":" + level.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: moves the user's matching SIGNAL objectives. A body with no quest tracker, or a
	//! client, reports nothing.
	//! \return True when any of the user's quests moved.
	static bool Report(IEntity user, string key)
	{
		if (!user || !Replication.IsServer())
			return false;

		LF_QuestManager manager = LF_QuestManager.GetInstance();
		if (!manager)
			return false;

		return manager.ReportSignal(user, key);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one signal per level a rise crossed, so a jump from level 1 to 3 counts level 2 too.
	static void ReportJobLevels(IEntity user, string job, int levelBefore, int level)
	{
		for (int reached = levelBefore + 1; reached <= level; reached++)
			Report(user, JobLevel(job, reached));
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether the prefab is one of the fish the fishing config lands.
	static bool IsFish(ResourceName prefab)
	{
		if (prefab.IsEmpty())
			return false;

		LF_FishingConfig config = LF_FishingConfig.Get();
		if (!config || !config.m_aSpecies)
			return false;

		foreach (LF_FishSpeciesDef species : config.m_aSpecies)
		{
			if (!species || !species.m_aSizePrefabs)
				continue;

			foreach (ResourceName fish : species.m_aSizePrefabs)
			{
				if (LF_Utils.SamePrefab(fish, prefab))
					return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the live body a player uid controls, or null (logged out, an AI, no such player).
	static IEntity BodyOfUid(string uid)
	{
		if (uid.IsEmpty() || !GetGame())
			return null;

		if (s_mTestBodies && s_mTestBodies.Contains(uid))
			return GetGame().GetWorld().FindEntityByID(s_mTestBodies.Get(uid));

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return null;

		array<int> playerIds = {};
		players.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			if (LF_Utils.GetPlayerUID(playerId) == uid)
				return players.GetPlayerControlledEntity(playerId);
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Test fixtures only: a body no player controls answers for a uid (BodyOfUid).
	static void SetBodyForTest(string uid, IEntity body)
	{
		if (!s_mTestBodies)
			s_mTestBodies = new map<string, EntityID>();

		if (body)
			s_mTestBodies.Set(uid, body.GetID());
		else
			s_mTestBodies.Remove(uid);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mTestBodies = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_QuestProgressSignals's statics (LF_StaticReset finds it by reflection).
class LF_QuestProgressSignals_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_QuestProgressSignals";
	}

	override void Apply()
	{
		LF_QuestProgressSignals.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! "vehicle_bought" when the lot sells a car (the buyer's own account paid and the car spawned).
modded class LF_VehicleLotComponent
{
	//------------------------------------------------------------------------------------------------
	override bool BuyColour(string buyerUid, ResourceName prefab, int colour, IEntity buyer, out string vehicleId, out int cashCharged, out string reasonKey)
	{
		bool bought = super.BuyColour(buyerUid, prefab, colour, buyer, vehicleId, cashCharged, reasonKey);
		if (bought)
			LF_QuestProgressSignals.Report(buyer, LF_QuestProgressSignals.VEHICLE_BOUGHT);

		return bought;
	}
}

//------------------------------------------------------------------------------------------------
//! "plot_bought" when a plot changes hands.
modded class LF_PlotComponent
{
	//------------------------------------------------------------------------------------------------
	override static LF_EPlotBuy Buy(IEntity buyer, string ownerKey, out string plotKey, string askedKey = "")
	{
		LF_EPlotBuy result = super.Buy(buyer, ownerKey, plotKey, askedKey);
		if (result == LF_EPlotBuy.BOUGHT)
			LF_QuestProgressSignals.Report(buyer, LF_QuestProgressSignals.PLOT_BOUGHT);

		return result;
	}
}

//------------------------------------------------------------------------------------------------
//! "permit_collected" when the licence desk hands the permit over (at once or at the collection visit).
modded class LF_LicenceDesk
{
	//------------------------------------------------------------------------------------------------
	override static LF_ELicenceDesk VisitFor(IEntity player, string uid, notnull LF_CashWallet wallet, out int minutesLeft)
	{
		LF_ELicenceDesk outcome = super.VisitFor(player, uid, wallet, minutesLeft);
		if (outcome == LF_ELicenceDesk.ISSUED)
			LF_QuestProgressSignals.Report(player, LF_QuestProgressSignals.PERMIT_COLLECTED);

		return outcome;
	}
}

//------------------------------------------------------------------------------------------------
//! "fish_sold" when a counter pays for a fish.
modded class LF_ShopComponent
{
	//------------------------------------------------------------------------------------------------
	override LF_EShopRefusal Sell(ResourceName prefab, int quantity, IEntity seller, out int earned, out string missing)
	{
		LF_EShopRefusal result = super.Sell(prefab, quantity, seller, earned, missing);
		if (result == LF_EShopRefusal.NONE && LF_QuestProgressSignals.IsFish(prefab))
			LF_QuestProgressSignals.Report(seller, LF_QuestProgressSignals.FISH_SOLD);

		return result;
	}
}

//------------------------------------------------------------------------------------------------
//! "job_level:<JOB>:<n>" when a character's level in a job rises, whatever granted the XP.
//! The level sync already subscribes to LF_OnJobLevelChanged, so no second subscription.
modded class LF_JobLevelSync
{
	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		bool handled = super.Handle(busEvent);
		LF_OnJobLevelChanged changed = LF_OnJobLevelChanged.Cast(busEvent);
		if (changed)
			LF_QuestProgressSignals.ReportJobLevels(LF_QuestProgressSignals.BodyOfUid(changed.m_sPlayerUid), changed.m_sJob, changed.m_iLevelBefore, changed.m_iLevel);

		return handled;
	}
}
