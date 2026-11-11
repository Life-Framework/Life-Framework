//------------------------------------------------------------------------------------------------
//! Server-side guards for client-requested job changes and job rewards.
//!
//! WORKPLACE. A client can name any job in RpcAsk_SetJob, so the server requires an entity
//! carrying an LF_JobSignUpAction for that job within WORKPLACE_RADIUS of the caller.
//! COOLDOWN. A client-requested switch needs SWITCH_COOLDOWN_SECONDS since the last switch.
//! EQUIPMENT. A job kit is handed out once per job per account per server session, so
//! switching back and forth (or respawning) does not mint tools.
//------------------------------------------------------------------------------------------------
class LF_JobGuards
{
	static const float WORKPLACE_RADIUS = 5.0;
	static const float SWITCH_COOLDOWN_SECONDS = 30.0;

	protected static ref set<string> s_aEquipmentGranted;

	protected static LF_EJobType s_eWantedJob;
	protected static bool s_bWorkplaceFound;

	//------------------------------------------------------------------------------------------------
	//! Server world time in seconds. 0 when no world is loaded.
	static float Now()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;

		return GetGame().GetWorld().GetWorldTime() / 1000.0;
	}

	//------------------------------------------------------------------------------------------------
	//! \param lastSwitchAt World time of the last switch in seconds. Negative means never.
	//! \return True when a new client-requested switch is allowed at now.
	static bool SwitchCooldownElapsed(float lastSwitchAt, float now)
	{
		if (lastSwitchAt < 0)
			return true;

		return now - lastSwitchAt >= SWITCH_COOLDOWN_SECONDS;
	}

	//------------------------------------------------------------------------------------------------
	//! Records that playerUid received the kit for job.
	//! \return True the first time per uid and job this session, false afterwards or on an empty uid.
	static bool ClaimEquipmentGrant(string playerUid, LF_EJobType job)
	{
		if (playerUid.IsEmpty())
			return false;

		if (!s_aEquipmentGranted)
			s_aEquipmentGranted = new set<string>();

		string key = playerUid + ":" + typename.EnumToString(LF_EJobType, job);
		if (s_aEquipmentGranted.Contains(key))
			return false;

		s_aEquipmentGranted.Insert(key);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when an entity with an LF_JobSignUpAction for job is within WORKPLACE_RADIUS.
	static bool IsNearWorkplace(vector position, LF_EJobType job)
	{
		if (!GetGame() || !GetGame().GetWorld())
			return false;

		s_eWantedJob = job;
		s_bWorkplaceFound = false;
		GetGame().GetWorld().QueryEntitiesBySphere(position, WORKPLACE_RADIUS, CollectWorkplace, null, EQueryEntitiesFlags.ALL);
		return s_bWorkplaceFound;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CollectWorkplace(IEntity entity)
	{
		if (!entity)
			return true;

		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(entity.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return true;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			LF_JobSignUpAction signUp = LF_JobSignUpAction.Cast(action);
			if (signUp && signUp.GetJob() == s_eWantedJob)
			{
				s_bWorkplaceFound = true;
				return false;
			}
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! One player's Fruit Catcher round, held by the server. Start issues a round id and stamps
//! the start time; Claim pays a round at most once, only for the fruit it was opened with,
//! and only after MIN_ROUND_SECONDS. A claim loop therefore earns at most one round's cap per
//! MIN_ROUND_SECONDS, the pace of honest play.
//------------------------------------------------------------------------------------------------
class LF_FruitCatcherRound : Managed
{
	//! Must not exceed the minigame's real round length when the minigame screen lands.
	static const float MIN_ROUND_SECONDS = 30.0;

	protected int m_iNextRoundId = 1;
	protected int m_iOpenRoundId;
	protected EHarvestJobType m_eFruit;
	protected float m_fStartedAt;

	//------------------------------------------------------------------------------------------------
	//! \return The fruit prefab a round of this type pays, or empty for an unknown type.
	static ResourceName FruitPrefab(EHarvestJobType fruit)
	{
		if (fruit == EHarvestJobType.APPLES)
			return "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
		if (fruit == EHarvestJobType.PLUMS)
			return "{C3E35D690317B5BE}Prefabs/Items/Food/Plum.et";
		if (fruit == EHarvestJobType.BERRIES)
			return "{73CF2BF72525F5D5}Prefabs/Items/Food/RowanBerries.et";

		return ResourceName.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Opens a new round, replacing any open one.
	//! \return The new round id (always above 0).
	int Start(EHarvestJobType fruit, float now)
	{
		m_iOpenRoundId = m_iNextRoundId;
		m_iNextRoundId++;
		m_eFruit = fruit;
		m_fStartedAt = now;
		return m_iOpenRoundId;
	}

	//------------------------------------------------------------------------------------------------
	bool HasOpenRound()
	{
		return m_iOpenRoundId > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Closes the round when the claim is valid.
	//! \return Empty when the claim is accepted, otherwise the refusal reason.
	string Claim(int roundId, EHarvestJobType fruit, float now)
	{
		if (m_iOpenRoundId <= 0)
			return "no open round";

		if (roundId != m_iOpenRoundId)
			return "id does not match the open round";

		if (fruit != m_eFruit)
			return "fruit type does not match the open round";

		if (now - m_fStartedAt < MIN_ROUND_SECONDS)
			return "ended before the minimum round time";

		m_iOpenRoundId = 0;
		return "";
	}
}
