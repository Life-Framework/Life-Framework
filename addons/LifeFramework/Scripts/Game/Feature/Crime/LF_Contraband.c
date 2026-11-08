//------------------------------------------------------------------------------------------------
//! One convoy. A member of the organization that owns the hideout hauls the
//! configured contraband from the start marker to the drop. Delivery pays cash
//! once. Leaving the vehicle or dying fails the run and logs it. A failed or
//! already-paid run pays nothing.
//------------------------------------------------------------------------------------------------
enum LF_EConvoyState
{
	NONE,
	RUNNING,
	PAID,
	FAILED
}

//------------------------------------------------------------------------------------------------
class LF_ConvoyVehicleWatch : Managed
{
	string m_sCharacterId;
	EntityID m_UserId;
	bool m_bMatchUser;
	EntityID m_VehicleId = EntityID.INVALID;

	//------------------------------------------------------------------------------------------------
	void Attach(IEntity user, IEntity vehicle)
	{
		if (!vehicle)
			return;

		EventHandlerManagerComponent events = EventHandlerManagerComponent.Cast(vehicle.FindComponent(EventHandlerManagerComponent));
		if (!events)
		{
			EL_Debug.Error("Contraband", "convoy vehicle has no event handler");
			return;
		}

		if (user)
		{
			m_UserId = user.GetID();
			m_bMatchUser = true;
		}

		events.RegisterScriptHandler("OnCompartmentLeft", this, OnCompartmentLeft);
		m_VehicleId = vehicle.GetID();
		EL_Debug.Log("Contraband", string.Format("convoy watching vehicle for %1", m_sCharacterId));
	}

	//------------------------------------------------------------------------------------------------
	//! Unregisters the OnCompartmentLeft handler. A vehicle already deleted took its handlers with it.
	void Detach()
	{
		if (m_VehicleId == EntityID.INVALID)
			return;

		EntityID vehicleId = m_VehicleId;
		m_VehicleId = EntityID.INVALID;
		if (!GetGame() || !GetGame().GetWorld())
			return;

		IEntity vehicle = GetGame().GetWorld().FindEntityByID(vehicleId);
		if (!vehicle)
			return;

		EventHandlerManagerComponent events = EventHandlerManagerComponent.Cast(vehicle.FindComponent(EventHandlerManagerComponent));
		if (events)
			events.RemoveScriptHandler("OnCompartmentLeft", this, OnCompartmentLeft);

		EL_Debug.Log("Contraband", string.Format("convoy stopped watching vehicle for %1", m_sCharacterId));
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCompartmentLeft(IEntity vehicle, BaseCompartmentManagerComponent mgr, IEntity occupant, int managerId, int slotID)
	{
		if (!occupant)
			return;

		if (m_bMatchUser && occupant.GetID() == m_UserId)
		{
			LF_Contraband.FailLeftVehicle(m_sCharacterId);
			return;
		}

		string occupantId = LF_Hideout.CharacterIdOf(occupant);
		if (!occupantId.IsEmpty() && occupantId == m_sCharacterId)
			LF_Contraband.FailLeftVehicle(m_sCharacterId);
	}
}

//------------------------------------------------------------------------------------------------
class LF_ConvoyRun : Managed
{
	string m_sCharacterId;
	LF_EConvoyState m_iState;
	int m_iCashPaid;
	EntityID m_UserId;
	bool m_bUserBound;
	ref LF_ConvoyVehicleWatch m_pWatch;
}

//------------------------------------------------------------------------------------------------
class LF_Contraband
{
	static const int PAYOUT = 200;
	static const ResourceName ITEM = "{73CF2BF72525F5D5}Prefabs/Items/Food/RowanBerries.et";
	static const string START_ID = "Roadmap_Convoy_Start";
	static const string DROP_ID = "Roadmap_Convoy_Drop";

	protected static ref map<string, ref LF_ConvoyRun> s_mRuns;
	protected static string s_sLastRefusal;

	//------------------------------------------------------------------------------------------------
	//! Drops every run (and unregisters its vehicle watch). Also the game-mode start/end reset.
	static void Reset()
	{
		if (s_mRuns)
		{
			foreach (string characterId, LF_ConvoyRun run : s_mRuns)
				StopWatch(run);
		}

		s_sLastRefusal = "";
		s_mRuns = new map<string, ref LF_ConvoyRun>();
	}

	//------------------------------------------------------------------------------------------------
	//! Disconnect: a running convoy fails (the hauler left) and the vehicle watch is dropped.
	//! \param characterId Active character id of the leaving player, may be empty.
	//! \param user Controlled entity of the leaving player, may be null.
	static void ReleasePlayer(string characterId, IEntity user)
	{
		if (!s_mRuns)
			return;

		string key = characterId;
		if ((key.IsEmpty() || !s_mRuns.Contains(key)) && user)
		{
			key = "";
			EntityID userId = user.GetID();
			foreach (string runKey, LF_ConvoyRun bound : s_mRuns)
			{
				if (bound && bound.m_bUserBound && bound.m_UserId == userId)
				{
					key = runKey;
					break;
				}
			}
		}

		LF_ConvoyRun run = Find(key);
		if (!run)
			return;

		Fail(key, "convoy failed: player disconnected");
		StopWatch(run);
	}

	//------------------------------------------------------------------------------------------------
	static string GetLastRefusal()
	{
		return s_sLastRefusal;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsContraband(ResourceName prefab)
	{
		if (prefab.IsEmpty())
			return false;

		return prefab == ITEM;
	}

	//------------------------------------------------------------------------------------------------
	//! The player-facing reason a character may not start the convoy, or empty when they may.
	//! No organization and an organization that does not hold the hideout read differently,
	//! so the player knows what to do next.
	static string StartRefusalKey(string characterId)
	{
		if (characterId.IsEmpty())
			return "#LF-Contraband_JoinOrg";

		EL_Organization org = EL_OrganizationManager.GetByCharacter(characterId);
		if (!org)
			return "#LF-Contraband_JoinOrg";

		string ownerId = LF_Hideout.GetOwner(LF_Hideout.HIDEOUT_ID);
		if (ownerId.IsEmpty() || org.GetId() != ownerId)
			return "#LF-Contraband_NotHideoutOwner";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	static bool Start(string characterId, IEntity user)
	{
		if (!Server())
			return false;

		if (!StartRefusalKey(characterId).IsEmpty())
		{
			Refuse("convoy refused: not a member");
			return false;
		}

		LF_ConvoyRun run = Ensure(characterId);
		if (run.m_iState != LF_EConvoyState.RUNNING)
		{
			run.m_iState = LF_EConvoyState.RUNNING;
			EL_Debug.Log("Contraband", string.Format("convoy started %1", characterId));
		}

		if (user)
		{
			run.m_UserId = user.GetID();
			run.m_bUserBound = true;
			WatchVehicle(run, user);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Pays the configured cash once when the run is active and the caller is
	//! carrying at least one of the configured contraband prefab. payTo null
	//! records the payout without spawning MoneyStack.
	static int Deliver(string characterId, int carried, IEntity payTo)
	{
		if (!Server())
			return 0;

		LF_ConvoyRun run = Find(characterId);
		if (!run || run.m_iState != LF_EConvoyState.RUNNING)
		{
			if (run && run.m_iState == LF_EConvoyState.PAID)
				Refuse("delivery refused: already paid");
			else if (run && run.m_iState == LF_EConvoyState.FAILED)
				Refuse("delivery refused: run failed");
			else
				Refuse("delivery refused: no run");
			return 0;
		}

		if (!IsMember(characterId))
		{
			Refuse("delivery refused: not a member");
			return 0;
		}

		if (carried < 1)
		{
			Refuse("delivery refused: no contraband");
			return 0;
		}

		int paid = PAYOUT;
		if (payTo)
		{
			int given = EL_MoneyUtils.GiveCash(payTo, PAYOUT);
			if (given <= 0)
			{
				Refuse("delivery refused: cash payout failed");
				return 0;
			}

			paid = given;
			int removed = EL_InventoryUtils.RemoveAmount(payTo, ITEM, 1);
			if (removed != 1)
				EL_Debug.Error("Contraband", "delivery removed no contraband after payment");
		}

		run.m_iState = LF_EConvoyState.PAID;
		run.m_iCashPaid = run.m_iCashPaid + paid;
		StopWatch(run);
		EL_Debug.Log("Contraband", string.Format("delivery paid %1 to %2", paid, characterId));
		if (GetGame() && GetGame().GetWorkspace())
			EL_Utils.NotifyEntity(payTo, "#EL-Contraband_Paid", "#EL-Contraband_Deliver", 3.0);
		return paid;
	}

	//------------------------------------------------------------------------------------------------
	static bool FailLeftVehicle(string characterId)
	{
		return Fail(characterId, "convoy failed: left the vehicle");
	}

	//------------------------------------------------------------------------------------------------
	static bool FailDied(string characterId)
	{
		return Fail(characterId, "convoy failed: died");
	}

	//------------------------------------------------------------------------------------------------
	static void FailDiedFromEntity(IEntity user)
	{
		if (!user || !s_mRuns)
			return;

		string characterId = LF_Hideout.CharacterIdOf(user);
		if (!characterId.IsEmpty())
		{
			FailDied(characterId);
			return;
		}

		EntityID userId = user.GetID();
		foreach (string key, LF_ConvoyRun run : s_mRuns)
		{
			if (!run || !run.m_bUserBound)
				continue;

			if (run.m_UserId == userId)
			{
				FailDied(key);
				return;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	static int GetCashPaid(string characterId)
	{
		LF_ConvoyRun run = Find(characterId);
		if (!run)
			return 0;

		return run.m_iCashPaid;
	}

	//------------------------------------------------------------------------------------------------
	static int CountCarried(IEntity user)
	{
		if (!user)
			return 0;

		int carried = EL_InventoryUtils.GetAmount(user, ITEM);
		if (carried < 0)
			return 0;

		return carried;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Fail(string characterId, string message)
	{
		LF_ConvoyRun run = Find(characterId);
		if (!run || run.m_iState != LF_EConvoyState.RUNNING)
			return false;

		run.m_iState = LF_EConvoyState.FAILED;
		StopWatch(run);
		Refuse(message);
		if (GetGame() && GetGame().GetWorkspace())
			EL_Utils.Notify("#EL-Contraband_Failed", "#EL-Contraband_Deliver", 3.0);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsMember(string characterId)
	{
		string ownerId = LF_Hideout.GetOwner(LF_Hideout.HIDEOUT_ID);
		EL_Organization org = EL_OrganizationManager.GetByCharacter(characterId);
		if (ownerId.IsEmpty() || !org)
			return false;

		return org.GetId() == ownerId;
	}

	//------------------------------------------------------------------------------------------------
	//! Unregisters the run's vehicle handler. The watch object itself is kept until the next
	//! WatchVehicle replaces it: Fail can run inside that watch's own OnCompartmentLeft, and
	//! dropping the last strong ref there would delete the object mid-call.
	protected static void StopWatch(LF_ConvoyRun run)
	{
		if (!run || !run.m_pWatch)
			return;

		run.m_pWatch.Detach();
	}

	//------------------------------------------------------------------------------------------------
	//! A (re)started run always watches the vehicle the user is in now, never a previous run's.
	protected static void WatchVehicle(LF_ConvoyRun run, IEntity user)
	{
		if (!run || !user)
			return;

		StopWatch(run);

		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (!character || !character.IsInVehicle())
			return;

		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
		if (!access)
			return;

		BaseCompartmentSlot slot = access.GetCompartment();
		if (!slot)
			return;

		IEntity vehicle = slot.GetOwner();
		if (!vehicle)
			return;

		ref LF_ConvoyVehicleWatch watch = new LF_ConvoyVehicleWatch();
		watch.m_sCharacterId = run.m_sCharacterId;
		watch.Attach(user, vehicle);
		run.m_pWatch = watch;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ConvoyRun Ensure(string characterId)
	{
		if (!s_mRuns)
			s_mRuns = new map<string, ref LF_ConvoyRun>();

		LF_ConvoyRun run = s_mRuns.Get(characterId);
		if (!run)
		{
			run = new LF_ConvoyRun();
			run.m_sCharacterId = characterId;
			run.m_iState = LF_EConvoyState.NONE;
			s_mRuns.Set(characterId, run);
		}

		return run;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ConvoyRun Find(string characterId)
	{
		if (!s_mRuns || characterId.IsEmpty())
			return null;

		return s_mRuns.Get(characterId);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Refuse(string message)
	{
		bool repeat = s_sLastRefusal == message;
		s_sLastRefusal = message;
		if (repeat)
			return;

		EL_Debug.Error("Contraband", message);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Server()
	{
		if (Replication.IsServer())
			return true;

		Refuse("rejected: not the server");
		return false;
	}
}
