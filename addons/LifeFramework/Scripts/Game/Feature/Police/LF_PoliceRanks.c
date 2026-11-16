//------------------------------------------------------------------------------------------------
//! Police rank ladder. LNGC has no rank ladder; this is not a port of faction select.
//!
//! Two ranks. Patrol officer draws the M9 and fines up to the patrol cap. Senior officer
//! draws the M16A2 and fines up to the existing maximum. Who may arrest and escort is config
//! (LF_PoliceConfig); the flagship opens both to every on-duty officer, so a fresh server
//! plays the arrest loop from the first minute. Promotion is server-side: an
//! on-duty officer promotes a lower one when the promoter outranks the rung the target moves
//! into (on a two-rung ladder, any senior), or 3600 seconds on duty promotes patrol.
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
	//! The outlet id the station armory reads (LF_ItemCatalogue.conf). Its issue offers carry the rank gate.
	static const string ARMORY_OUTLET = "police_armory";

	static const ResourceName SIDEARM = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";
	static const ResourceName LONG_GUN = "{3E413771E1834D2F}Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et";
	//! The folded spike strip (#646): every rank carries one.
	static const ResourceName SPIKE_STRIP = "{4B8BDB727DEE59B8}Prefabs/Props/Roleplay/SpikeStrip/SpikeStrip_Item.et";

	protected static int s_iArrestRefusals;
	protected static int s_iFineRefusals;
	protected static int s_iSelfPromotionRefusals;
	protected static int s_iMissingPrefabSkips;
	protected static string s_sLastError;
	protected static ref array<EntityID> s_aBoundIds;
	protected static ref array<string> s_aBoundUids;

	//------------------------------------------------------------------------------------------------
	//! Game-mode start/end reset (LF_StaticReset). EntityIDs bound in one world mean nothing in the next.
	static void ResetStatics()
	{
		s_iArrestRefusals = 0;
		s_iFineRefusals = 0;
		s_iSelfPromotionRefusals = 0;
		s_iMissingPrefabSkips = 0;
		s_sLastError = "";
		s_aBoundIds = null;
		s_aBoundUids = null;
	}

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
		LF_PoliceRank row = Row(rank);
		if (row && !row.m_sTitleKey.IsEmpty())
			return row.m_sTitleKey;

		return "EL-Rank_Patrol";
	}

	//------------------------------------------------------------------------------------------------
	//! Rungs on the configured ladder (the built-in two when the config gives none).
	static int RankCount()
	{
		array<ref LF_PoliceRank> ranks = {};
		LF_PoliceConfig.Ranks(ranks);
		return ranks.Count();
	}

	//------------------------------------------------------------------------------------------------
	static int TopRank()
	{
		return RankCount() - 1;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsRankUsable(int rank)
	{
		return rank >= 0 && rank < RankCount();
	}

	//------------------------------------------------------------------------------------------------
	//! Fine cap of the rank. An index past either end of the ladder clamps to the nearest rung.
	static int FineCap(int rank)
	{
		LF_PoliceRank row = Row(rank);
		if (!row)
			return 0;

		return row.m_iFineCap;
	}

	//------------------------------------------------------------------------------------------------
	//! Pay per on-duty second for the rank; negative means the rank defers to the jobs config rate.
	static float PayPerSecond(int rank)
	{
		LF_PoliceRank row = Row(rank);
		if (!row)
			return -1;

		return row.m_fPayPerSecond;
	}

	//------------------------------------------------------------------------------------------------
	//! On-duty seconds that promote this rank to the next. 0 for the top rank.
	static int DutySecondsForNext(int rank)
	{
		if (rank >= TopRank())
			return 0;

		LF_PoliceRank row = Row(rank);
		if (!row)
			return 0;

		return Math.Max(row.m_iDutySecondsForNext, 0);
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
	static bool CanViewWantedList(LF_PlayerAccount account)
	{
		if (!account)
			return false;

		if (account.GetFaction() != LF_Faction.POLICE)
			return false;

		return account.IsOnDuty();
	}

	//------------------------------------------------------------------------------------------------
	static bool CanDraw(string uid)
	{
		LF_PlayerAccount account = Account(uid);
		if (!OnDutyPolice(account))
			return false;

		return IsRankUsable(account.GetPoliceRank());
	}

	//------------------------------------------------------------------------------------------------
	static bool CanPromote(string uid)
	{
		LF_PlayerAccount account = Account(uid);
		if (!OnDutyPolice(account))
			return false;

		int rank = account.GetPoliceRank();
		if (rank <= PATROL)
			return false;

		// The lowest promotion there is (rung 0 into rung 1) needs this rank at least.
		return rank >= PromoterRankNeeded(PATROL);
	}

	//------------------------------------------------------------------------------------------------
	//! Lowest rank that may promote an officer now at targetRank: one above the rung the target is
	//! promoted into. The top rung is the ceiling, so the top rank still promotes into the top rung
	//! and a two-rung ladder keeps "the senior promotes the patrol".
	static int PromoterRankNeeded(int targetRank)
	{
		return Math.Min(targetRank + 2, TopRank());
	}

	//------------------------------------------------------------------------------------------------
	//! Pure rule: promoterRank outranks the rung targetRank is promoted into.
	static bool PromoterOutranksNewRank(int promoterRank, int targetRank)
	{
		return promoterRank > targetRank && promoterRank >= PromoterRankNeeded(targetRank);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure rank check: rank meets the configured minimum.
	static bool RankMeets(int rank, int minRank)
	{
		return rank >= minRank;
	}

	//------------------------------------------------------------------------------------------------
	static bool CanArrestRank(int rank)
	{
		return RankMeets(rank, LF_PoliceConfig.ArrestMinRank());
	}

	//------------------------------------------------------------------------------------------------
	static bool CanEscortRank(int rank)
	{
		return RankMeets(rank, LF_PoliceConfig.EscortMinRank());
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
	static bool AuthorizeArrest(LF_PlayerAccount account)
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
	static bool AuthorizeFine(LF_PlayerAccount account, int amount)
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

		LF_PlayerAccount promoter = Account(promoterUid);
		LF_PlayerAccount target = Account(targetUid);
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

		if (target.GetFaction() != LF_Faction.POLICE)
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

		// The server's rule, whatever the client offered: the promoter must hold a rank above the
		// rung the target moves into (the top rank is the ceiling). Saved ranks are indexes.
		if (!PromoterOutranksNewRank(promoter.GetPoliceRank(), target.GetPoliceRank()))
		{
			s_sLastError = string.Format("promote refused: rank %1 does not outrank new rank %2 (needs rank %3)", promoter.GetPoliceRank(), nextRank, PromoterRankNeeded(target.GetPoliceRank()));
			LF_Debug.Warn("Police", s_sLastError);
			NotifyOfficer(BoundEntity(promoterUid), "#EL-Rank_PromoteNeedsHigher");
			return false;
		}

		target.SetPoliceRank(nextRank);
		LF_PoliceApplications.EndTrial(targetUid);
		LF_Debug.Log("Ranks", string.Format("promoted %1 to rank %2 by %3", targetUid, nextRank, promoterUid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds on-duty seconds for a police account. Promotes one step when the threshold is met.
	//! \return true when this call promoted the officer.
	static bool AccrueDuty(string uid, int seconds)
	{
		if (seconds <= 0)
			return false;

		LF_PlayerAccount account = Account(uid);
		if (!account)
		{
			Note("duty accrual refused: no account");
			return false;
		}

		if (account.GetFaction() != LF_Faction.POLICE)
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
		LF_PoliceApplications.EndTrial(uid);
		LF_Debug.Log("Ranks", string.Format("promoted %1 to rank %2 by time on duty", uid, nextRank));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void OnDutyChanged(LF_PlayerAccount account, bool nowOnDuty)
	{
		OnDutyChangedAt(account, nowOnDuty, NowMs());
	}

	//------------------------------------------------------------------------------------------------
	//! OnDutyChanged with the world time passed in, so a test can drive the clock.
	static void OnDutyChangedAt(LF_PlayerAccount account, bool nowOnDuty, int nowMs)
	{
		if (!account)
			return;

		// A shift boundary in either direction opens the next shift's single armory draw.
		account.SetArmoryDrawnThisShift(false);

		if (nowOnDuty)
		{
			account.StartDutyClock(nowMs);
			LF_Debug.Log("Ranks", string.Format("duty clock started for %1", account.GetPersistentId()));
			return;
		}

		SettleDutyAt(account, false, nowMs);
	}

	//------------------------------------------------------------------------------------------------
	//! Gives the officer the resolved kit and nothing else. Refusals are logged.
	static bool Draw(string uid, IEntity officer)
	{
		LF_PlayerAccount account = Account(uid);
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

		if (account.HasDrawnArmoryThisShift())
		{
			// One kit per duty shift. Without this an officer could draw kits without end and
			// hand them to anyone. Going off duty opens the next shift's draw.
			s_sLastError = string.Format("draw refused: %1 already drew a kit this shift", uid);
			LF_Debug.Warn("Ranks", s_sLastError);
			NotifyOfficer(officer, "#EL-Rank_AlreadyDrawn");
			return false;
		}

		SettleDutyAt(account, true, NowMs());

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
		int expected = kit.Count();
		foreach (ResourceName prefab : kit)
		{
			// A kit line the armory outlet issues is gated and served by the outlet (#1763): free,
			// serialled, and refused below the offer's rank. Other lines (spike strip, roadblock kit) are plain.
			LF_CatalogueItem issueItem;
			LF_OutletOffer issueOffer = LF_ItemCatalogue.OfferFor(LF_ItemCatalogueConfig.Get(), ARMORY_OUTLET, prefab, issueItem);
			if (issueOffer && issueOffer.m_bIssue && issueItem)
			{
				string refusedBy;
				if (IssueOffer(uid, officer, issueOffer, issueItem, refusedBy))
					inserted++;
				else if (!refusedBy.IsEmpty())
					expected--;

				continue;
			}

			int added = LF_InventoryUtils.AddAmount(officer, prefab, 1);
			if (added != 1)
			{
				Note(string.Format("kit line not inserted: %1", prefab));
				continue;
			}

			inserted++;
		}

		// Any weapon handed out spends the shift's draw, so a partly full inventory cannot be
		// retried for more kit lines.
		if (inserted > 0)
			account.SetArmoryDrawnThisShift(true);

		if (inserted != expected)
			return false;

		LF_Debug.Log("Ranks", string.Format("kit drawn for %1 rank %2 lines %3", uid, account.GetPoliceRank(), inserted));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One item from the police_armory outlet by catalogue id (#1763). Free: no money moves. The
	//! outlet's gates decide (job and rank); a refusal names what is missing, the rank title as a
	//! localization key in `missing`. A weapon is booked to the officer's account as a serial.
	//! \param[out] missing What the officer lacks, empty when issued.
	static bool IssueItem(string uid, IEntity officer, string itemId, out string missing)
	{
		missing = string.Empty;
		LF_ItemCatalogueConfig config = LF_ItemCatalogueConfig.Get();
		LF_CatalogueItem item = LF_ItemCatalogue.Item(config, itemId);
		LF_OutletOffer offer;
		if (config)
			offer = config.FindOffer(ARMORY_OUTLET, itemId);

		if (!item || !offer || offer.m_bRemoved || !offer.m_bIssue)
		{
			Note(string.Format("issue refused: the armory does not issue %1", itemId));
			return false;
		}

		return IssueOffer(uid, officer, offer, item, missing);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IssueOffer(string uid, IEntity officer, notnull LF_OutletOffer offer, notnull LF_CatalogueItem item, out string missing)
	{
		missing = string.Empty;
		LF_PlayerAccount account = Account(uid);
		if (!OnDutyPolice(account))
		{
			Note(string.Format("issue refused: %1 is not on-duty police", uid));
			return false;
		}

		LF_PoliceIssueFacts facts = new LF_PoliceIssueFacts();
		facts.m_iRank = account.GetPoliceRank();
		LF_EOutletGate gate = LF_ItemCatalogue.Check(offer, facts, missing);
		if (gate != LF_EOutletGate.OPEN)
		{
			s_sLastError = string.Format("issue refused: %1 at rank %2 lacks %3 (%4) for %5", uid, facts.m_iRank, typename.EnumToString(LF_EOutletGate, gate), missing, offer.m_sItem);
			LF_Debug.Warn("Ranks", s_sLastError);
			if (gate == LF_EOutletGate.NEEDS_RANK)
				NotifyOfficerArg(officer, "#EL-Rank_IssueNeedsRank", missing);
			return false;
		}

		if (!officer || !Replication.IsServer())
		{
			Note("issue refused: no officer entity on the server");
			return false;
		}

		ResourceName prefab = offer.m_IssuePrefab;
		if (prefab.IsEmpty())
			prefab = item.m_Prefab;

		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(officer);
		IEntity issued = LF_Utils.SpawnEntityPrefab(prefab, officer.GetOrigin());
		if (!issued || !inventory || !inventory.TryInsertItem(issued))
		{
			if (issued)
				SCR_EntityHelper.DeleteEntityAndChildren(issued);

			Note(string.Format("issue refused: %1 did not fit on the officer", offer.m_sItem));
			return false;
		}

		LF_SerialComponent stamp = LF_SerialComponent.Cast(issued.FindComponent(LF_SerialComponent));
		if (stamp)
		{
			string serial = LF_FenceSerials.Issue(uid, prefab);
			if (serial.IsEmpty() || !stamp.SetSerial(serial))
				LF_Debug.Error("Ranks", string.Format("issued %1 to %2 without a serial", offer.m_sItem, uid));
		}
		else
		{
			LF_Debug.Error("Ranks", string.Format("issued %1 carries no serial component; it is untracked", prefab));
		}

		LF_Debug.Log("Ranks", string.Format("issued %1 to %2 rank %3", offer.m_sItem, uid, facts.m_iRank));
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

		string uid = LF_Utils.GetPlayerUid(user);
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
		LF_PlayerAccount promoterAccount = Account(promoterUid);
		if (!promoterAccount)
			return "";

		string refused;
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

			LF_PlayerAccount otherAccount = Account(otherUid);
			if (!otherAccount || otherAccount.GetFaction() != LF_Faction.POLICE)
				continue;

			if (otherAccount.GetPoliceRank() >= promoterAccount.GetPoliceRank())
				continue;

			// Prefer someone this promoter may actually promote; otherwise Promote refuses the
			// nearest lower officer and tells the promoter why.
			if (PromoterOutranksNewRank(promoterAccount.GetPoliceRank(), otherAccount.GetPoliceRank()))
				return otherUid;

			if (refused.IsEmpty())
				refused = otherUid;
		}

		return refused;
	}

	//------------------------------------------------------------------------------------------------
	protected static void RankPrefabs(int rank, notnull array<ResourceName> prefabs)
	{
		prefabs.Clear();
		LF_PoliceRank row = Row(rank);
		if (!row || !row.m_aKit)
			return;

		foreach (ResourceName prefab : row.m_aKit)
		{
			prefabs.Insert(prefab);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The ladder row for a rank, clamped to the ladder so a rank saved under a longer ladder
	//! reads as the top rung.
	protected static LF_PoliceRank Row(int rank)
	{
		array<ref LF_PoliceRank> ranks = {};
		LF_PoliceConfig.Ranks(ranks);
		if (ranks.IsEmpty())
			return null;

		return ranks.Get(Math.ClampInt(rank, 0, ranks.Count() - 1));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Catalog(notnull array<ResourceName> prefabs)
	{
		prefabs.Clear();
		prefabs.Insert(SIDEARM);
		prefabs.Insert(LONG_GUN);
		prefabs.Insert(SPIKE_STRIP);
		prefabs.Insert(LF_Roadblocks.KIT);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ContainsPrefab(notnull array<ResourceName> prefabs, ResourceName candidate)
	{
		foreach (ResourceName prefab : prefabs)
		{
			if (LF_Utils.SamePrefab(prefab, candidate))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool OnDutyPolice(LF_PlayerAccount account)
	{
		if (!account)
			return false;

		if (account.GetFaction() != LF_Faction.POLICE)
			return false;

		return account.IsOnDuty();
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_PlayerAccount Account(string uid)
	{
		if (uid.IsEmpty())
			return null;

		LF_PlayerAccountManager manager = LF_PlayerAccountManager.GetInstance();
		if (!manager)
			return null;

		return manager.GetAccount(uid);
	}

	//------------------------------------------------------------------------------------------------
	//! Closes the running duty clock at nowMs: the seconds count toward rank and toward the
	//! shift's unpaid duty pay (police only). stillOnDuty restarts the clock at nowMs.
	static void SettleDutyAt(LF_PlayerAccount account, bool stillOnDuty, int nowMs)
	{
		if (!account || !account.IsDutyClockRunning())
			return;

		int started = account.GetDutyStartedAtMs();
		if (nowMs > started)
		{
			int deltaSeconds = (nowMs - started) / 1000;
			if (deltaSeconds > 0)
			{
				// Rank seconds are police only; a medic's seconds are only the shift's pay.
				if (account.GetFaction() == LF_Faction.POLICE)
					AccrueDuty(account.GetPersistentId(), deltaSeconds);

				if (account.GetFaction() != LF_Faction.CIVILIAN)
					account.AddUnpaidDutySeconds(deltaSeconds);
			}
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
	protected static void NotifyOfficer(IEntity officer, string messageKey)
	{
		if (!officer || !GetGame() || !GetGame().GetPlayerManager())
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(officer);
		if (playerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-Police_Title", messageKey, 3.0, LF_ENotificationType.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	protected static void NotifyOfficerArg(IEntity officer, string messageKey, string arg)
	{
		if (!officer || !GetGame() || !GetGame().GetPlayerManager())
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(officer);
		if (playerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-Police_Title", messageKey, 3.0, LF_ENotificationType.WARNING, arg);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Note(string message)
	{
		s_sLastError = message;
		LF_Debug.Error("Ranks", message);
	}

	//------------------------------------------------------------------------------------------------
	//! The entity BindOfficer tied to this uid, or null (a test account, a despawned officer).
	protected static IEntity BoundEntity(string uid)
	{
		if (uid.IsEmpty() || !s_aBoundIds || !GetGame() || !GetGame().GetWorld())
			return null;

		for (int i = s_aBoundUids.Count() - 1; i >= 0; i--)
		{
			if (s_aBoundUids.Get(i) == uid)
				return GetGame().GetWorld().FindEntityByID(s_aBoundIds.Get(i));
		}

		return null;
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

//------------------------------------------------------------------------------------------------
//! What the armory outlet's gates ask about an officer who is already known to be on duty police:
//! just the rank. No world, so the issue rule reads the same on the server and in a test.
class LF_PoliceIssueFacts : LF_BuyerFacts
{
	int m_iRank;

	//------------------------------------------------------------------------------------------------
	override int JobRank(string job)
	{
		if (job == LF_EntityBuyerFacts.JOB_POLICE)
			return m_iRank;

		return -1;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PoliceRanks's statics (registered at script load; Core/LF_StaticReset.c).
class LF_PoliceRanks_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_PoliceRanks_StaticResetHook());

	override string Owner()
	{
		return "LF_PoliceRanks";
	}

	override void Apply()
	{
		LF_PoliceRanks.ResetStatics();
	}
}
