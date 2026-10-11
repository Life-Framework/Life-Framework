//------------------------------------------------------------------------------------------------
//! Police rank ladder. LNGC has no rank ladder; this is not a port of faction select.
//!
//! Two ranks. Patrol officer draws the M9 and cannot arrest. Senior officer draws the
//! M16A2, fines up to the existing cap, and can arrest. Promotion is server-side: an
//! on-duty senior promotes a lower officer, or 3600 seconds on duty promotes patrol.
//! RequestSelfPromotion is the RPC body and always refuses. Rank does not whitelist
//! anyone and is not a license.
//!
//! A kit line whose prefab does not load is skipped and logged. The rank stays usable.
//------------------------------------------------------------------------------------------------
class LF_PoliceRanks
{
	static const int PATROL = 0;
	static const int SENIOR = 1;
	static const int PATROL_FINE_CAP = 1000;
	static const int DUTY_SECONDS = 3600;
	static const float PROMOTE_RADIUS = 5;

	static const ResourceName SIDEARM = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";
	static const ResourceName LONG_GUN = "{3E413771E1834D2F}Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et";

	protected static int s_iArrestRefusals;
	protected static int s_iFineRefusals;
	protected static int s_iSelfPromotionRefusals;
	protected static int s_iMissingPrefabSkips;
	protected static string s_sLastError;
	protected static ref array<EntityID> s_aBoundIds;
	protected static ref array<string> s_aBoundUids;

	//------------------------------------------------------------------------------------------------
	static int GetArrestRefusalCount()
	{
		return s_iArrestRefusals;
	}

	//------------------------------------------------------------------------------------------------
	static int GetFineRefusalCount()
	{
		return s_iFineRefusals;
	}

	//------------------------------------------------------------------------------------------------
	static int GetSelfPromotionRefusalCount()
	{
		return s_iSelfPromotionRefusals;
	}

	//------------------------------------------------------------------------------------------------
	static int GetMissingPrefabSkips()
	{
		return s_iMissingPrefabSkips;
	}

	//------------------------------------------------------------------------------------------------
	static string GetLastError()
	{
		return s_sLastError;
	}

	//------------------------------------------------------------------------------------------------
	static string TitleKey(int rank)
	{
		if (rank == SENIOR)
			return "EL-Rank_Senior";

		return "EL-Rank_Patrol";
	}

	//------------------------------------------------------------------------------------------------
	static bool IsRankUsable(int rank)
	{
		if (rank == PATROL)
			return true;

		return rank == SENIOR;
	}

	//------------------------------------------------------------------------------------------------
	static int FineCap(int rank)
	{
		if (rank >= SENIOR)
			return EL_PoliceUtils.MAX_FINE;

		return PATROL_FINE_CAP;
	}

	//------------------------------------------------------------------------------------------------
	static int DutySecondsForNext(int rank)
	{
		if (rank == PATROL)
			return DUTY_SECONDS;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure threshold. requiredSeconds of 0 never promotes (the top rank).
	static bool PromotionDue(int onDutySeconds, int requiredSeconds)
	{
		if (requiredSeconds <= 0)
			return false;

		return onDutySeconds >= requiredSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! Lower ranks still open the wanted list. Rank is not part of this gate.
	static bool CanViewWantedList(EL_PlayerAccount account)
	{
		if (!account)
			return false;

		if (account.GetFaction() != EL_Faction.POLICE)
			return false;

		return account.IsOnDuty();
	}

	//------------------------------------------------------------------------------------------------
	static bool CanDraw(string uid)
	{
		EL_PlayerAccount account = Account(uid);
		if (!OnDutyPolice(account))
			return false;

		return IsRankUsable(account.GetPoliceRank());
	}

	//------------------------------------------------------------------------------------------------
	static bool CanPromote(string uid)
	{
		EL_PlayerAccount account = Account(uid);
		if (!OnDutyPolice(account))
			return false;

		return account.GetPoliceRank() > PATROL;
	}

	//------------------------------------------------------------------------------------------------
	static bool CanArrestRank(int rank)
	{
		return rank >= SENIOR;
	}

	//------------------------------------------------------------------------------------------------
	//! Server body of the self-promotion RPC. Always refuses and does not change rank.
	static bool RequestSelfPromotion(string uid)
	{
		s_iSelfPromotionRefusals++;
		Note(string.Format("self-promotion refused: %1", uid));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	static bool AuthorizeArrest(EL_PlayerAccount account)
	{
		if (!OnDutyPolice(account))
		{
			s_iArrestRefusals++;
			Note("arrest refused: not an on-duty officer");
			return false;
		}

		if (!CanArrestRank(account.GetPoliceRank()))
		{
			s_iArrestRefusals++;
			Note(string.Format("arrest refused: rank %1 cannot arrest", account.GetPoliceRank()));
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool AuthorizeFine(EL_PlayerAccount account, int amount)
	{
		if (!OnDutyPolice(account))
		{
			s_iFineRefusals++;
			Note("fine refused: not an on-duty officer");
			return false;
		}

		int cap = FineCap(account.GetPoliceRank());
		if (amount <= 0 || amount > cap)
		{
			s_iFineRefusals++;
			Note(string.Format("fine refused: amount %1 above rank cap %2", amount, cap));
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Prefabs that are on both lists, in left-list order, without duplicates.
	static void Intersect(notnull array<ResourceName> left, notnull array<ResourceName> right, notnull array<ResourceName> result)
	{
		result.Clear();
		foreach (ResourceName candidate : left)
		{
			if (!ContainsPrefab(right, candidate))
				continue;

			if (ContainsPrefab(result, candidate))
				continue;

			result.Insert(candidate);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Drops prefabs that do not load. Each drop is logged. The caller keeps the rank.
	static void FilterLoadable(notnull array<ResourceName> configured, notnull array<ResourceName> kept)
	{
		kept.Clear();
		foreach (ResourceName prefab : configured)
		{
			if (prefab == ResourceName.Empty || !Resource.Load(prefab).IsValid())
			{
				s_iMissingPrefabSkips++;
				Note(string.Format("kit line skipped: prefab did not load %1", prefab));
				continue;
			}

			kept.Insert(prefab);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Resolved kit: intersection of that rank's list and the station catalog, then loadable lines.
	static void CollectKit(int rank, notnull array<ResourceName> kit)
	{
		array<ResourceName> rankPrefabs = {};
		RankPrefabs(rank, rankPrefabs);
		array<ResourceName> catalog = {};
		Catalog(catalog);
		array<ResourceName> allowed = {};
		Intersect(rankPrefabs, catalog, allowed);
		FilterLoadable(allowed, kit);
	}

	//------------------------------------------------------------------------------------------------
	static bool Promote(string promoterUid, string targetUid)
	{
		if (promoterUid.IsEmpty() || targetUid.IsEmpty())
		{
			Note("promote refused: empty player id");
			return false;
		}

		if (promoterUid == targetUid)
			return RequestSelfPromotion(promoterUid);

		EL_PlayerAccount promoter = Account(promoterUid);
		EL_PlayerAccount target = Account(targetUid);
		if (!promoter || !target)
		{
			Note("promote refused: missing account");
			return false;
		}

		if (!OnDutyPolice(promoter))
		{
			Note("promote refused: promoter is not on-duty police");
			return false;
		}

		if (target.GetFaction() != EL_Faction.POLICE)
		{
			Note("promote refused: target is not police");
			return false;
		}

		if (promoter.GetPoliceRank() <= target.GetPoliceRank())
		{
			Note("promote refused: promoter does not outrank the target");
			return false;
		}

		int nextRank = target.GetPoliceRank() + 1;
		if (!IsRankUsable(nextRank))
		{
			Note("promote refused: target has no higher rank");
			return false;
		}

		target.SetPoliceRank(nextRank);
		EL_Debug.Log("Ranks", string.Format("promoted %1 to rank %2 by %3", targetUid, nextRank, promoterUid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds on-duty seconds for a police account. Promotes one step when the threshold is met.
	//! \return true when this call promoted the officer.
	static bool AccrueDuty(string uid, int seconds)
	{
		if (seconds <= 0)
			return false;

		EL_PlayerAccount account = Account(uid);
		if (!account)
		{
			Note("duty accrual refused: no account");
			return false;
		}

		if (account.GetFaction() != EL_Faction.POLICE)
		{
			Note("duty accrual refused: not police");
			return false;
		}

		account.AddOnDutySeconds(seconds);
		int required = DutySecondsForNext(account.GetPoliceRank());
		if (!PromotionDue(account.GetOnDutySeconds(), required))
			return false;

		int nextRank = account.GetPoliceRank() + 1;
		if (!IsRankUsable(nextRank))
			return false;

		account.SetPoliceRank(nextRank);
		EL_Debug.Log("Ranks", string.Format("promoted %1 to rank %2 by time on duty", uid, nextRank));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void OnDutyChanged(EL_PlayerAccount account, bool nowOnDuty)
	{
		if (!account)
			return;

		if (nowOnDuty)
		{
			account.StartDutyClock(NowMs());
			EL_Debug.Log("Ranks", string.Format("duty clock started for %1", account.GetPersistentId()));
			return;
		}

		SettleDuty(account, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Gives the officer the resolved kit and nothing else. Refusals are logged.
	static bool Draw(string uid, IEntity officer)
	{
		EL_PlayerAccount account = Account(uid);
		if (!OnDutyPolice(account))
		{
			Note(string.Format("draw refused: %1 is not on-duty police", uid));
			return false;
		}

		if (!IsRankUsable(account.GetPoliceRank()))
		{
			Note(string.Format("draw refused: rank %1 is not usable", account.GetPoliceRank()));
			return false;
		}

		SettleDuty(account, true);

		array<ResourceName> kit = {};
		CollectKit(account.GetPoliceRank(), kit);
		if (kit.Count() == 0)
		{
			Note(string.Format("draw refused: rank %1 has no loadable kit", account.GetPoliceRank()));
			return false;
		}

		if (!officer)
		{
			Note("draw refused: no officer entity");
			return false;
		}

		if (!Replication.IsServer())
		{
			Note("draw refused: not the server");
			return false;
		}

		int inserted = 0;
		foreach (ResourceName prefab : kit)
		{
			int added = EL_InventoryUtils.AddAmount(officer, prefab, 1);
			if (added != 1)
			{
				Note(string.Format("kit line not inserted: %1", prefab));
				continue;
			}

			inserted++;
		}

		if (inserted != kit.Count())
			return false;

		EL_Debug.Log("Ranks", string.Format("kit drawn for %1 rank %2 lines %3", uid, account.GetPoliceRank(), inserted));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void BindOfficer(IEntity officer, string uid)
	{
		if (!officer || uid.IsEmpty())
		{
			Note("bind refused: officer or uid missing");
			return;
		}

		if (!s_aBoundIds)
		{
			s_aBoundIds = new array<EntityID>();
			s_aBoundUids = new array<string>();
		}

		EntityID id = officer.GetID();
		s_aBoundIds.Insert(id);
		s_aBoundUids.Insert(uid);
	}

	//------------------------------------------------------------------------------------------------
	static string ResolveUid(IEntity user)
	{
		if (!user)
			return "";

		string uid = EL_Utils.GetPlayerUid(user);
		if (!uid.IsEmpty())
			return uid;

		return BoundUid(user);
	}

	//------------------------------------------------------------------------------------------------
	static string FindLowerRankNearby(IEntity promoter, float radius)
	{
		if (!promoter || !s_aBoundIds || !GetGame() || !GetGame().GetWorld())
			return "";

		string promoterUid = ResolveUid(promoter);
		EL_PlayerAccount promoterAccount = Account(promoterUid);
		if (!promoterAccount)
			return "";

		for (int i = s_aBoundIds.Count() - 1; i >= 0; i--)
		{
			IEntity other = GetGame().GetWorld().FindEntityByID(s_aBoundIds.Get(i));
			if (!other || other == promoter)
				continue;

			if (vector.Distance(promoter.GetOrigin(), other.GetOrigin()) > radius)
				continue;

			string otherUid = s_aBoundUids.Get(i);
			if (otherUid.IsEmpty() || otherUid == promoterUid)
				continue;

			EL_PlayerAccount otherAccount = Account(otherUid);
			if (!otherAccount || otherAccount.GetFaction() != EL_Faction.POLICE)
				continue;

			if (otherAccount.GetPoliceRank() >= promoterAccount.GetPoliceRank())
				continue;

			return otherUid;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected static void RankPrefabs(int rank, notnull array<ResourceName> prefabs)
	{
		prefabs.Clear();
		if (rank == PATROL)
			prefabs.Insert(SIDEARM);
		else if (rank == SENIOR)
			prefabs.Insert(LONG_GUN);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Catalog(notnull array<ResourceName> prefabs)
	{
		prefabs.Clear();
		prefabs.Insert(SIDEARM);
		prefabs.Insert(LONG_GUN);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ContainsPrefab(notnull array<ResourceName> prefabs, ResourceName candidate)
	{
		foreach (ResourceName prefab : prefabs)
		{
			if (prefab == candidate)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool OnDutyPolice(EL_PlayerAccount account)
	{
		if (!account)
			return false;

		if (account.GetFaction() != EL_Faction.POLICE)
			return false;

		return account.IsOnDuty();
	}

	//------------------------------------------------------------------------------------------------
	protected static EL_PlayerAccount Account(string uid)
	{
		if (uid.IsEmpty())
			return null;

		EL_PlayerAccountManager manager = EL_PlayerAccountManager.GetInstance();
		if (!manager)
			return null;

		return manager.GetAccount(uid);
	}

	//------------------------------------------------------------------------------------------------
	protected static void SettleDuty(EL_PlayerAccount account, bool stillOnDuty)
	{
		if (!account || !account.IsDutyClockRunning())
			return;

		int started = account.GetDutyStartedAtMs();
		int nowMs = NowMs();
		if (nowMs > started)
		{
			int deltaSeconds = (nowMs - started) / 1000;
			if (deltaSeconds > 0)
				AccrueDuty(account.GetPersistentId(), deltaSeconds);
		}

		if (stillOnDuty)
			account.StartDutyClock(nowMs);
		else
			account.StopDutyClock();
	}

	//------------------------------------------------------------------------------------------------
	protected static int NowMs()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;

		return Math.Round(GetGame().GetWorld().GetWorldTime());
	}

	//------------------------------------------------------------------------------------------------
	protected static void Note(string message)
	{
		s_sLastError = message;
		EL_Debug.Error("Ranks", message);
	}

	//------------------------------------------------------------------------------------------------
	protected static string BoundUid(IEntity officer)
	{
		if (!officer || !s_aBoundIds)
			return "";

		EntityID id = officer.GetID();
		for (int i = s_aBoundIds.Count() - 1; i >= 0; i--)
		{
			if (s_aBoundIds.Get(i) == id)
				return s_aBoundUids.Get(i);
		}

		return "";
	}
}
