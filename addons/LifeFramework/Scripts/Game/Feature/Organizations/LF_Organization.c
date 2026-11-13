//------------------------------------------------------------------------------------------------
//! A persisted organization: stable id, name, ranks, members, a shared fund, and
//! upgrade flags. LNGC has no organization scripts (docs/roadmap/sources.md). The
//! fund follows LF_BankAccount: a balance keyed by organization id. It is not a
//! second currency and not a personal ATM balance. Cash enters and leaves only
//! through LF_OrganizationPlayer, which moves MoneyStack.
//!
//! Ranks (#750a) are a ladder: position 0 is the top (the founder), and a rank carries a rights
//! bitmask (LF_OrgRights). Rank changes and removals only go down the ladder.
//!
//! Identity is PersistenceIdUtils, never an EntityID. One membership per character.
//! Load replaces the cache, so ApplyAll is idempotent on a live session.
//------------------------------------------------------------------------------------------------
enum LF_EOrganizationUpgrade
{
	EXTRA_STORAGE,
	PROCESSING_SLOT,
	PARKED_VEHICLE
}

//------------------------------------------------------------------------------------------------
//! The value is the bit index in a rank's rights mask. Append only: bits 0-3 are the four bools
//! saved before #750a, and a saved mask keeps its meaning only while no value moves.
enum LF_EOrganizationPermission
{
	WITHDRAW,
	INVITE,
	KICK,
	BUY_UPGRADE,
	//! Promote and demote members below you on the ladder.
	MANAGE_RANKS,
	//! Reserved for the war record (#750b).
	DECLARE_WAR,
	//! Reserved: the hideout stash still opens for every member.
	USE_STASH,
	//! Reserved for territory (#751).
	MANAGE_TERRITORY,
	//! Reserved: spending the fund on something other than an upgrade.
	SPEND_FUND
}

//------------------------------------------------------------------------------------------------
//! The core default ladder. A new organization gets all three rungs; a version 1 organization
//! (founder and member only) gets the officer rung on load.
class LF_OrganizationConfig
{
	static const string RANK_FOUNDER = "founder";
	static const string RANK_OFFICER = "officer";
	static const string RANK_MEMBER = "member";

	//------------------------------------------------------------------------------------------------
	//! \param[out] ids Default rank ids, top first.
	static void DefaultLadder(notnull array<string> ids)
	{
		ids.Clear();
		ids.Insert(RANK_FOUNDER);
		ids.Insert(RANK_OFFICER);
		ids.Insert(RANK_MEMBER);
	}

	//------------------------------------------------------------------------------------------------
	//! Display name (a localization key) of a default rank, or empty for any other id.
	static string DefaultName(string rankId)
	{
		if (rankId == RANK_FOUNDER)
			return "#LF-Org_RankFounder";
		if (rankId == RANK_OFFICER)
			return "#LF-Org_RankOfficer";
		if (rankId == RANK_MEMBER)
			return "#LF-Org_RankMember";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	static int DefaultRights(string rankId)
	{
		if (rankId == RANK_FOUNDER)
			return LF_OrgRights.ALL;
		if (rankId == RANK_OFFICER)
			return LF_OrgRights.Bit(LF_EOrganizationPermission.INVITE) | LF_OrgRights.Bit(LF_EOrganizationPermission.KICK) | LF_OrgRights.Bit(LF_EOrganizationPermission.USE_STASH);
		if (rankId == RANK_MEMBER)
			return LF_OrgRights.Bit(LF_EOrganizationPermission.USE_STASH);

		return 0;
	}
}

//------------------------------------------------------------------------------------------------
//! The rank as the "organizations" payload has saved it since save version 2. Field order is the
//! binary layout and must not change: a pre-#750a save is read through this class. The four bools
//! are bits 0-3 of the rank's rights, written so an older build still reads a newer save; the
//! whole mask and the ladder order ride the "orgranks" payload (LF_OrganizationRankRow).
class LF_OrganizationRankRecord
{
	string m_sId;
	string m_sName;
	bool m_bCanWithdraw;
	bool m_bCanInvite;
	bool m_bCanKick;
	bool m_bCanBuyUpgrade;

	//------------------------------------------------------------------------------------------------
	static LF_OrganizationRankRecord Create(string id, string name, bool canWithdraw, bool canInvite, bool canKick, bool canBuyUpgrade)
	{
		LF_OrganizationRankRecord record();
		record.m_sId = id;
		record.m_sName = name;
		record.m_bCanWithdraw = canWithdraw;
		record.m_bCanInvite = canInvite;
		record.m_bCanKick = canKick;
		record.m_bCanBuyUpgrade = canBuyUpgrade;
		return record;
	}
}

//------------------------------------------------------------------------------------------------
//! One rank's ladder position and full rights mask ("orgranks" payload, save version 27, #750a).
//! Keyed by organization id and rank id, both stable strings. Field order is the binary layout.
//! Version is first.
class LF_OrganizationRankRow
{
	static const int VERSION = 1;

	int m_iVersion;
	string m_sOrgId;
	string m_sRankId;
	int m_iOrder;
	int m_iRights;

	//------------------------------------------------------------------------------------------------
	static LF_OrganizationRankRow Create(string orgId, string rankId, int order, int rights)
	{
		LF_OrganizationRankRow row();
		row.m_iVersion = VERSION;
		row.m_sOrgId = orgId;
		row.m_sRankId = rankId;
		row.m_iOrder = order;
		row.m_iRights = rights;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
class LF_OrganizationMemberRecord
{
	string m_sCharacterId;
	string m_sRankId;

	//------------------------------------------------------------------------------------------------
	static LF_OrganizationMemberRecord Create(string characterId, string rankId)
	{
		LF_OrganizationMemberRecord record();
		record.m_sCharacterId = characterId;
		record.m_sRankId = rankId;
		return record;
	}
}

//------------------------------------------------------------------------------------------------
//! Field order is the binary layout. Version is first. Version 1: written before #750a, when a rank
//! was its four bools; load back-fills the rights mask and adds the officer rung. Version 2: the
//! same layout, and the save also carries the "orgranks" rows that hold the whole mask and order.
class LF_OrganizationRecord
{
	static const int VERSION = 2;

	int m_iVersion;
	string m_sId;
	string m_sName;
	int m_iFund;
	bool m_bExtraStorage;
	bool m_bProcessingSlot;
	bool m_bParkedVehicle;
	ref array<ref LF_OrganizationRankRecord> m_aRanks = {};
	ref array<ref LF_OrganizationMemberRecord> m_aMembers = {};

	//------------------------------------------------------------------------------------------------
	static LF_OrganizationRecord Create(string id, string name, int fund, bool extraStorage, bool processingSlot, bool parkedVehicle)
	{
		LF_OrganizationRecord record();
		record.m_iVersion = VERSION;
		record.m_sId = id;
		record.m_sName = name;
		record.m_iFund = fund;
		record.m_bExtraStorage = extraStorage;
		record.m_bProcessingSlot = processingSlot;
		record.m_bParkedVehicle = parkedVehicle;
		record.m_aRanks = {};
		record.m_aMembers = {};
		return record;
	}
}

//------------------------------------------------------------------------------------------------
class LF_OrganizationRank : Managed
{
	protected string m_sId;
	protected string m_sName;
	//! Ladder position: 0 is the top. LF_Organization keeps it equal to the index in its ladder.
	protected int m_iOrder;
	protected int m_iRights;

	//------------------------------------------------------------------------------------------------
	string GetId()
	{
		return m_sId;
	}

	//------------------------------------------------------------------------------------------------
	string GetName()
	{
		return m_sName;
	}

	//------------------------------------------------------------------------------------------------
	int GetOrder()
	{
		return m_iOrder;
	}

	//------------------------------------------------------------------------------------------------
	int GetRights()
	{
		return m_iRights;
	}

	//------------------------------------------------------------------------------------------------
	bool Allows(LF_EOrganizationPermission permission)
	{
		return LF_OrgRights.Has(m_iRights, permission);
	}

	//------------------------------------------------------------------------------------------------
	void SetOrder(int order)
	{
		m_iOrder = order;
	}

	//------------------------------------------------------------------------------------------------
	void SetRights(int rights)
	{
		m_iRights = LF_OrgRights.Clean(rights);
	}

	//------------------------------------------------------------------------------------------------
	void SetName(string name)
	{
		m_sName = name;
	}

	//------------------------------------------------------------------------------------------------
	//! The positional record: bits 0-3 as the four bools.
	LF_OrganizationRankRecord ToRecord()
	{
		return LF_OrganizationRankRecord.Create(m_sId, m_sName, Allows(LF_EOrganizationPermission.WITHDRAW), Allows(LF_EOrganizationPermission.INVITE),
			Allows(LF_EOrganizationPermission.KICK), Allows(LF_EOrganizationPermission.BUY_UPGRADE));
	}

	//------------------------------------------------------------------------------------------------
	LF_OrganizationRankRow ToRow(string orgId)
	{
		return LF_OrganizationRankRow.Create(orgId, m_sId, m_iOrder, m_iRights);
	}

	//------------------------------------------------------------------------------------------------
	//! The rank from its record alone: the four bools as a mask (LF_OrgRights.MigrateLegacy). A save
	//! with "orgranks" rows then sets the whole mask and the order (LF_Organization.ApplyRankRow).
	static LF_OrganizationRank FromRecord(LF_OrganizationRankRecord record, int order)
	{
		if (!record)
			return null;

		int rights = LF_OrgRights.MigrateLegacy(record.m_sId, record.m_bCanWithdraw, record.m_bCanInvite, record.m_bCanKick, record.m_bCanBuyUpgrade);
		return Create(record.m_sId, record.m_sName, order, rights);
	}

	//------------------------------------------------------------------------------------------------
	static LF_OrganizationRank Create(string id, string name, int order, int rights)
	{
		LF_OrganizationRank rank();
		rank.m_sId = id;
		rank.m_sName = name;
		rank.m_iOrder = order;
		rank.m_iRights = LF_OrgRights.Clean(rights);
		return rank;
	}
}

//------------------------------------------------------------------------------------------------
class LF_OrganizationMember : Managed
{
	protected string m_sCharacterId;
	protected string m_sRankId;

	//------------------------------------------------------------------------------------------------
	string GetCharacterId()
	{
		return m_sCharacterId;
	}

	//------------------------------------------------------------------------------------------------
	string GetRankId()
	{
		return m_sRankId;
	}

	//------------------------------------------------------------------------------------------------
	void SetRankId(string rankId)
	{
		m_sRankId = rankId;
	}

	//------------------------------------------------------------------------------------------------
	LF_OrganizationMemberRecord ToRecord()
	{
		return LF_OrganizationMemberRecord.Create(m_sCharacterId, m_sRankId);
	}

	//------------------------------------------------------------------------------------------------
	static LF_OrganizationMember Create(string characterId, string rankId)
	{
		LF_OrganizationMember member();
		member.m_sCharacterId = characterId;
		member.m_sRankId = rankId;
		return member;
	}

	//------------------------------------------------------------------------------------------------
	static LF_OrganizationMember FromRecord(LF_OrganizationMemberRecord record)
	{
		if (!record)
			return null;

		return Create(record.m_sCharacterId, record.m_sRankId);
	}
}

//------------------------------------------------------------------------------------------------
class LF_Organization : Managed
{
	protected string m_sId;
	protected string m_sName;
	protected int m_iFund;
	protected bool m_bExtraStorage;
	protected bool m_bProcessingSlot;
	protected bool m_bParkedVehicle;
	protected ref array<ref LF_OrganizationRank> m_aRanks = {};
	protected ref array<ref LF_OrganizationMember> m_aMembers = {};

	//------------------------------------------------------------------------------------------------
	string GetId()
	{
		return m_sId;
	}

	//------------------------------------------------------------------------------------------------
	string GetName()
	{
		return m_sName;
	}

	//------------------------------------------------------------------------------------------------
	int GetFund()
	{
		return m_iFund;
	}

	//------------------------------------------------------------------------------------------------
	bool HasExtraStorage()
	{
		return m_bExtraStorage;
	}

	//------------------------------------------------------------------------------------------------
	bool HasProcessingSlot()
	{
		return m_bProcessingSlot;
	}

	//------------------------------------------------------------------------------------------------
	bool HasParkedVehicle()
	{
		return m_bParkedVehicle;
	}

	//------------------------------------------------------------------------------------------------
	int GetMemberCount()
	{
		return m_aMembers.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Character ids in join order (founder first).
	void GetMemberIds(notnull array<string> outIds)
	{
		outIds.Clear();
		foreach (LF_OrganizationMember member : m_aMembers)
		{
			if (member)
				outIds.Insert(member.GetCharacterId());
		}
	}

	//------------------------------------------------------------------------------------------------
	bool IsMember(string characterId)
	{
		return !RankIdOf(characterId).IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	string GetRankId(string characterId)
	{
		return RankIdOf(characterId);
	}

	//------------------------------------------------------------------------------------------------
	bool HasUpgrade(LF_EOrganizationUpgrade upgrade)
	{
		if (upgrade == LF_EOrganizationUpgrade.EXTRA_STORAGE)
			return m_bExtraStorage;
		if (upgrade == LF_EOrganizationUpgrade.PROCESSING_SLOT)
			return m_bProcessingSlot;
		if (upgrade == LF_EOrganizationUpgrade.PARKED_VEHICLE)
			return m_bParkedVehicle;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	void Credit(int amount)
	{
		if (amount > 0)
			m_iFund += amount;
	}

	//------------------------------------------------------------------------------------------------
	bool Debit(int amount)
	{
		if (amount <= 0)
			return false;
		if (m_iFund < amount)
			return false;

		m_iFund -= amount;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void SetFund(int value)
	{
		if (value < 0)
		{
			LF_Debug.Error("Organizations", string.Format("fund clamped: org=%1 value=%2", m_sId, value));
			m_iFund = 0;
			return;
		}

		m_iFund = value;
	}

	//------------------------------------------------------------------------------------------------
	bool Allows(string characterId, LF_EOrganizationPermission permission)
	{
		LF_OrganizationRank rank = FindRank(RankIdOf(characterId));
		if (!rank)
			return false;

		return rank.Allows(permission);
	}

	//------------------------------------------------------------------------------------------------
	//! The rights mask of a character's rank, 0 for a non-member.
	int RightsOf(string characterId)
	{
		LF_OrganizationRank rank = FindRank(RankIdOf(characterId));
		if (!rank)
			return 0;

		return rank.GetRights();
	}

	//------------------------------------------------------------------------------------------------
	//! Ladder position of a character's rank (0 is the top), or -1 for a non-member or a member whose
	//! rank is not in the table.
	int RankPosition(string characterId)
	{
		LF_OrganizationRank rank = FindRank(RankIdOf(characterId));
		if (!rank)
			return -1;

		return m_aRanks.Find(rank);
	}

	//------------------------------------------------------------------------------------------------
	int GetRankCount()
	{
		return m_aRanks.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[out] outIds Rank ids, top of the ladder first.
	void GetRankIds(notnull array<string> outIds)
	{
		outIds.Clear();
		foreach (LF_OrganizationRank rank : m_aRanks)
		{
			outIds.Insert(rank.GetId());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Rank id at a ladder position, or empty.
	string RankIdAt(int position)
	{
		if (position < 0 || position >= m_aRanks.Count())
			return string.Empty;

		return m_aRanks.Get(position).GetId();
	}

	//------------------------------------------------------------------------------------------------
	//! Rights mask of a rank, or 0 when the organization has no such rank.
	int GetRankRights(string rankId)
	{
		LF_OrganizationRank rank = FindRank(rankId);
		if (!rank)
			return 0;

		return rank.GetRights();
	}

	//------------------------------------------------------------------------------------------------
	//! Display name of a character's rank (a localization key for the default ranks), or empty.
	string GetRankName(string characterId)
	{
		LF_OrganizationRank rank = FindRank(RankIdOf(characterId));
		if (!rank)
			return string.Empty;

		return rank.GetName();
	}

	//------------------------------------------------------------------------------------------------
	//! True when actor's rank is strictly above target's. Equal ranks outrank nobody. A member whose
	//! rank is not in the table (a damaged save) sits below every rung, so they can still be removed.
	bool Outranks(string actorCharacterId, string targetCharacterId)
	{
		int actor = RankPosition(actorCharacterId);
		if (actor < 0 || !IsMember(targetCharacterId))
			return false;

		int target = RankPosition(targetCharacterId);
		if (target < 0)
			return true;

		return actor < target;
	}

	//------------------------------------------------------------------------------------------------
	//! The rung a new member joins on: the bottom of the ladder.
	string EntryRankId()
	{
		if (m_aRanks.IsEmpty())
			return string.Empty;

		return m_aRanks.Get(m_aRanks.Count() - 1).GetId();
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a member on another rank of this organization. Rights are the manager's check.
	bool SetMemberRank(string characterId, string rankId)
	{
		if (!FindRank(rankId))
			return false;

		foreach (LF_OrganizationMember member : m_aMembers)
		{
			if (member && member.GetCharacterId() == characterId)
			{
				member.SetRankId(rankId);
				return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Load: one "orgranks" row sets a rank's whole rights mask and its order. Call SortLadder after
	//! the last row. \return false when this organization has no such rank.
	bool ApplyRankRow(notnull LF_OrganizationRankRow row)
	{
		LF_OrganizationRank rank = FindRank(row.m_sRankId);
		if (!rank)
			return false;

		rank.SetOrder(row.m_iOrder);
		rank.SetRights(row.m_iRights);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Sorts the ranks by order (stable, so equal orders keep their saved sequence) and renumbers
	//! them 0..n-1, so a rank's order is always its ladder position.
	void SortLadder()
	{
		for (int i = 1; i < m_aRanks.Count(); i++)
		{
			int j = i;
			while (j > 0 && m_aRanks.Get(j - 1).GetOrder() > m_aRanks.Get(j).GetOrder())
			{
				m_aRanks.SwapItems(j - 1, j);
				j--;
			}
		}

		for (int position = 0; position < m_aRanks.Count(); position++)
		{
			m_aRanks.Get(position).SetOrder(position);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A version 1 organization had a founder and a member rank, each four bools. FromRecord already
	//! turned the bools into masks (LF_OrgRights.MigrateLegacy); this adds the default rungs it lacks
	//! (the officer), names the default ranks by their localization key and orders the ladder
	//! founder, officer, member, then any other rank in its saved sequence. Members keep their rank.
	//! \return The number of rungs added.
	int MigrateLegacyLadder()
	{
		array<string> ladder = {};
		LF_OrganizationConfig.DefaultLadder(ladder);
		int added = 0;
		foreach (string rankId : ladder)
		{
			if (FindRank(rankId))
				continue;

			m_aRanks.Insert(LF_OrganizationRank.Create(rankId, LF_OrganizationConfig.DefaultName(rankId), 0, LF_OrganizationConfig.DefaultRights(rankId)));
			added++;
		}

		int extra = ladder.Count();
		foreach (LF_OrganizationRank rank : m_aRanks)
		{
			int position = ladder.Find(rank.GetId());
			if (position < 0)
			{
				rank.SetOrder(extra);
				extra++;
				continue;
			}

			rank.SetOrder(position);
			rank.SetName(LF_OrganizationConfig.DefaultName(rank.GetId()));
		}

		SortLadder();
		return added;
	}

	//------------------------------------------------------------------------------------------------
	bool AddMember(string characterId, string rankId)
	{
		if (characterId.IsEmpty() || rankId.IsEmpty())
			return false;
		if (IsMember(characterId))
			return false;
		if (!FindRank(rankId))
			return false;

		m_aMembers.Insert(LF_OrganizationMember.Create(characterId, rankId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool RemoveMember(string characterId)
	{
		for (int i = m_aMembers.Count() - 1; i >= 0; i--)
		{
			LF_OrganizationMember member = m_aMembers.Get(i);
			if (!member)
				continue;
			if (member.GetCharacterId() != characterId)
				continue;

			m_aMembers.RemoveOrdered(i);
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	void SetUpgrade(LF_EOrganizationUpgrade upgrade, bool enabled)
	{
		if (upgrade == LF_EOrganizationUpgrade.EXTRA_STORAGE)
			m_bExtraStorage = enabled;
		else if (upgrade == LF_EOrganizationUpgrade.PROCESSING_SLOT)
			m_bProcessingSlot = enabled;
		else if (upgrade == LF_EOrganizationUpgrade.PARKED_VEHICLE)
			m_bParkedVehicle = enabled;
	}

	//------------------------------------------------------------------------------------------------
	LF_OrganizationRecord ToRecord()
	{
		LF_OrganizationRecord record = LF_OrganizationRecord.Create(m_sId, m_sName, m_iFund, m_bExtraStorage, m_bProcessingSlot, m_bParkedVehicle);
		foreach (LF_OrganizationRank rank : m_aRanks)
		{
			if (!rank)
				continue;

			record.m_aRanks.Insert(rank.ToRecord());
		}

		foreach (LF_OrganizationMember member : m_aMembers)
		{
			if (!member)
				continue;

			record.m_aMembers.Insert(member.ToRecord());
		}

		return record;
	}

	//------------------------------------------------------------------------------------------------
	static LF_Organization CreateNew(string id, string name, string founderCharacterId)
	{
		LF_Organization org();
		org.m_sId = id;
		org.m_sName = name;
		org.m_iFund = 0;
		org.m_aRanks = {};
		org.m_aMembers = {};
		array<string> ladder = {};
		LF_OrganizationConfig.DefaultLadder(ladder);
		foreach (int position, string rankId : ladder)
		{
			org.m_aRanks.Insert(LF_OrganizationRank.Create(rankId, LF_OrganizationConfig.DefaultName(rankId), position, LF_OrganizationConfig.DefaultRights(rankId)));
		}

		org.AddMember(founderCharacterId, LF_OrganizationConfig.RANK_FOUNDER);
		return org;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[out] outRows One "orgranks" row per rank, top first.
	void ExportRankRows(notnull array<ref LF_OrganizationRankRow> outRows)
	{
		foreach (LF_OrganizationRank rank : m_aRanks)
		{
			if (rank)
				outRows.Insert(rank.ToRow(m_sId));
		}
	}

	//------------------------------------------------------------------------------------------------
	static LF_Organization FromRecord(LF_OrganizationRecord record)
	{
		if (!record || record.m_sId.IsEmpty())
			return null;

		LF_Organization org();
		org.m_sId = record.m_sId;
		org.m_sName = record.m_sName;
		org.m_aRanks = {};
		org.m_aMembers = {};
		org.SetFund(record.m_iFund);
		org.m_bExtraStorage = record.m_bExtraStorage;
		org.m_bProcessingSlot = record.m_bProcessingSlot;
		org.m_bParkedVehicle = record.m_bParkedVehicle;

		// The saved sequence is the ladder (top first) until "orgranks" rows say otherwise.
		if (record.m_aRanks)
		{
			foreach (LF_OrganizationRankRecord rankRecord : record.m_aRanks)
			{
				LF_OrganizationRank rank = LF_OrganizationRank.FromRecord(rankRecord, org.m_aRanks.Count());
				if (!rank || rank.GetId().IsEmpty())
					continue;
				if (org.FindRank(rank.GetId()))
				{
					LF_Debug.Warn("Organizations", string.Format("load dropped a second rank %1 in %2", rank.GetId(), record.m_sId));
					continue;
				}

				org.m_aRanks.Insert(rank);
			}
		}

		if (record.m_aMembers)
		{
			foreach (LF_OrganizationMemberRecord memberRecord : record.m_aMembers)
			{
				LF_OrganizationMember member = LF_OrganizationMember.FromRecord(memberRecord);
				if (!member || member.GetCharacterId().IsEmpty())
					continue;

				org.m_aMembers.Insert(member);
			}
		}

		return org;
	}

	//------------------------------------------------------------------------------------------------
	protected string RankIdOf(string characterId)
	{
		foreach (LF_OrganizationMember member : m_aMembers)
		{
			if (!member)
				continue;
			if (member.GetCharacterId() == characterId)
				return member.GetRankId();
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_OrganizationRank FindRank(string rankId)
	{
		if (rankId.IsEmpty())
			return null;

		foreach (LF_OrganizationRank rank : m_aRanks)
		{
			if (!rank)
				continue;
			if (rank.GetId() == rankId)
				return rank;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
class LF_OrganizationManager : Managed
{
	static const string RANK_FOUNDER = "founder";
	static const string RANK_MEMBER = "member";

	protected static ref LF_OrganizationManager s_Instance;
	protected ref map<string, ref LF_Organization> m_mOrganizations = new map<string, ref LF_Organization>();
	protected ref map<string, string> m_mMembership = new map<string, string>();

	//------------------------------------------------------------------------------------------------
	static bool IsValidAmount(int amount)
	{
		if (amount <= 0)
			return false;

		return amount <= LF_OrgConfig.MaxCashMove();
	}

	//------------------------------------------------------------------------------------------------
	static int UpgradePrice(LF_EOrganizationUpgrade upgrade)
	{
		if (upgrade == LF_EOrganizationUpgrade.EXTRA_STORAGE)
			return LF_OrgConfig.PriceExtraStorage();
		if (upgrade == LF_EOrganizationUpgrade.PROCESSING_SLOT)
			return LF_OrgConfig.PriceProcessingSlot();
		if (upgrade == LF_EOrganizationUpgrade.PARKED_VEHICLE)
			return LF_OrgConfig.PriceParkedVehicle();

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	static string UpgradeFlagName(LF_EOrganizationUpgrade upgrade)
	{
		if (upgrade == LF_EOrganizationUpgrade.EXTRA_STORAGE)
			return "extra-storage";
		if (upgrade == LF_EOrganizationUpgrade.PROCESSING_SLOT)
			return "processing-slot";
		if (upgrade == LF_EOrganizationUpgrade.PARKED_VEHICLE)
			return "parked-vehicle";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	static LF_OrganizationManager GetInstance()
	{
		if (!s_Instance)
			s_Instance = new LF_OrganizationManager();

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	static void Reset()
	{
		if (s_Instance)
		{
			s_Instance.m_mOrganizations.Clear();
			s_Instance.m_mMembership.Clear();
		}

		s_Instance = null;
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_OrganizationRecord> ExportAll()
	{
		array<ref LF_OrganizationRecord> records = new array<ref LF_OrganizationRecord>();
		if (!s_Instance)
			return records;

		foreach (string orgId, LF_Organization org : s_Instance.m_mOrganizations)
		{
			if (!org)
				continue;

			records.Insert(org.ToRecord());
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! The "orgranks" payload: every rank's whole rights mask and ladder order.
	static array<ref LF_OrganizationRankRow> ExportRanks()
	{
		array<ref LF_OrganizationRankRow> rows = new array<ref LF_OrganizationRankRow>();
		if (!s_Instance)
			return rows;

		foreach (string orgId, LF_Organization org : s_Instance.m_mOrganizations)
		{
			if (org)
				org.ExportRankRows(rows);
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces every organization. A rank's rights start from its record's four bools
	//! (LF_OrgRights.MigrateLegacy); a version 1 record also gets the default rungs it lacks. A save
	//! from version 27 on then sets the whole masks and the order through ApplyRanks.
	static void ApplyAll(notnull array<ref LF_OrganizationRecord> records)
	{
		LF_OrganizationManager instance = GetInstance();
		instance.m_mOrganizations.Clear();
		instance.m_mMembership.Clear();

		int migrated = 0;
		int rungs = 0;
		foreach (LF_OrganizationRecord record : records)
		{
			if (!record)
				continue;
			if (record.m_iVersion < 1 || record.m_sId.IsEmpty())
			{
				LF_Debug.Error("Organizations", "load skipped a record with no id or version");
				continue;
			}

			LF_Organization org = LF_Organization.FromRecord(record);
			if (!org)
				continue;

			if (record.m_iVersion < 2)
			{
				rungs += org.MigrateLegacyLadder();
				migrated++;
			}

			instance.m_mOrganizations.Set(org.GetId(), org);
			instance.IndexMembers(org);
		}

		if (migrated > 0)
			LF_Debug.Info("Organizations", string.Format("load migrated %1 organization(s) from record version 1: rank bools became rights masks, %2 default rung(s) added, members kept their rank", migrated, rungs));
	}

	//------------------------------------------------------------------------------------------------
	//! Sets each saved rank's rights mask and order. Runs after ApplyAll in the same load, so it
	//! sets values and never adds: a second pass gives the same ladder. A row whose organization or
	//! rank is gone is dropped with a warning; a rank with no row keeps the mask from its record.
	static void ApplyRanks(notnull array<ref LF_OrganizationRankRow> rows)
	{
		LF_OrganizationManager instance = GetInstance();
		set<string> touched = new set<string>();
		int applied = 0;
		int dropped = 0;
		foreach (LF_OrganizationRankRow row : rows)
		{
			if (!row || row.m_iVersion < 1 || row.m_sOrgId.IsEmpty() || row.m_sRankId.IsEmpty())
			{
				LF_Debug.Error("Organizations", "rank load skipped a row with no version, organization or rank");
				dropped++;
				continue;
			}

			LF_Organization org = instance.m_mOrganizations.Get(row.m_sOrgId);
			if (!org)
			{
				LF_Debug.Warn("Organizations", string.Format("rank load dropped %1: organization %2 is gone", row.m_sRankId, row.m_sOrgId));
				dropped++;
				continue;
			}
			if (!org.ApplyRankRow(row))
			{
				LF_Debug.Warn("Organizations", string.Format("rank load dropped %1: %2 has no such rank", row.m_sRankId, row.m_sOrgId));
				dropped++;
				continue;
			}

			touched.Insert(row.m_sOrgId);
			applied++;
		}

		foreach (string orgId : touched)
		{
			LF_Organization sorted = instance.m_mOrganizations.Get(orgId);
			if (sorted)
				sorted.SortLadder();
		}

		LF_Debug.Info("Organizations", string.Format("rank load applied %1 row(s) to %2 organization(s), dropped %3", applied, touched.Count(), dropped));
	}

	//------------------------------------------------------------------------------------------------
	static LF_Organization Get(string orgId)
	{
		if (!s_Instance || orgId.IsEmpty())
			return null;

		return s_Instance.m_mOrganizations.Get(orgId);
	}

	//------------------------------------------------------------------------------------------------
	static LF_Organization GetByCharacter(string characterId)
	{
		if (!s_Instance || characterId.IsEmpty())
			return null;

		string orgId = s_Instance.m_mMembership.Get(characterId);
		if (orgId.IsEmpty())
			return null;

		return s_Instance.m_mOrganizations.Get(orgId);
	}

	//------------------------------------------------------------------------------------------------
	//! Case and outer spaces do not make a second organization: "North Dock" takes "north dock ".
	static bool IsNameTaken(string name)
	{
		if (!s_Instance)
			return false;

		string key = LF_OrgRules.NameKey(name);
		foreach (string orgId, LF_Organization org : s_Instance.m_mOrganizations)
		{
			if (org && LF_OrgRules.NameKey(org.GetName()) == key)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Name rules: LF_OrgRules.RefuseName, then unique (IsNameTaken). The name is stored trimmed.
	static LF_Organization Create(string founderCharacterId, string name)
	{
		if (!Server())
			return null;
		if (founderCharacterId.IsEmpty())
		{
			LF_Debug.Error("Organizations", "create refused: empty character id");
			return null;
		}
		string refusal = LF_OrgRules.RefuseName(name);
		if (!refusal.IsEmpty())
		{
			LF_Debug.Error("Organizations", string.Format("create refused: name '%1' (%2)", name, refusal));
			return null;
		}
		if (IsNameTaken(name))
		{
			LF_Debug.Error("Organizations", string.Format("create refused: name '%1' is already registered", name));
			return null;
		}
		if (GetByCharacter(founderCharacterId))
		{
			LF_Debug.Error("Organizations", string.Format("create refused: %1 already belongs to an organization", founderCharacterId));
			return null;
		}

		string orgId = PersistenceIdUtils.Generate();
		if (orgId.IsEmpty())
		{
			LF_Debug.Error("Organizations", "create refused: persistence id was empty");
			return null;
		}

		LF_OrganizationManager instance = GetInstance();
		LF_Organization org = LF_Organization.CreateNew(orgId, name.Trim(), founderCharacterId);
		instance.m_mOrganizations.Set(orgId, org);
		instance.m_mMembership.Set(founderCharacterId, orgId);
		LF_Debug.Log("Organizations", string.Format("created org=%1 name=%2 founder=%3", orgId, org.GetName(), founderCharacterId));
		return org;
	}

	//------------------------------------------------------------------------------------------------
	static bool Invite(string orgId, string actorCharacterId, string targetCharacterId)
	{
		if (!Server())
			return false;

		LF_Organization org = Get(orgId);
		if (!org)
		{
			LF_Debug.Error("Organizations", string.Format("invite refused: no organization %1", orgId));
			return false;
		}
		if (targetCharacterId.IsEmpty())
		{
			LF_Debug.Error("Organizations", "invite refused: empty character id");
			return false;
		}
		if (!org.Allows(actorCharacterId, LF_EOrganizationPermission.INVITE))
		{
			LF_Debug.Error("Organizations", string.Format("invite refused: %1 lacks invite rank", actorCharacterId));
			return false;
		}
		if (GetByCharacter(targetCharacterId))
		{
			LF_Debug.Error("Organizations", string.Format("invite refused: %1 already belongs to an organization", targetCharacterId));
			return false;
		}
		if (!org.AddMember(targetCharacterId, org.EntryRankId()))
		{
			LF_Debug.Error("Organizations", string.Format("invite refused: %1 was not added", targetCharacterId));
			return false;
		}

		GetInstance().m_mMembership.Set(targetCharacterId, orgId);
		LF_Debug.Log("Organizations", string.Format("invite org=%1 member=%2", orgId, targetCharacterId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool Kick(string orgId, string actorCharacterId, string targetCharacterId)
	{
		if (!Server())
			return false;

		LF_Organization org = Get(orgId);
		if (!org)
		{
			LF_Debug.Error("Organizations", string.Format("kick refused: no organization %1", orgId));
			return false;
		}
		if (!org.IsMember(targetCharacterId))
		{
			LF_Debug.Error("Organizations", string.Format("kick refused: %1 is not a member", targetCharacterId));
			return false;
		}
		if (!org.Allows(actorCharacterId, LF_EOrganizationPermission.KICK))
		{
			LF_Debug.Error("Organizations", string.Format("kick refused: %1 lacks kick rank", actorCharacterId));
			return false;
		}
		if (org.Allows(targetCharacterId, LF_EOrganizationPermission.KICK) && targetCharacterId == actorCharacterId)
		{
			LF_Debug.Error("Organizations", "kick refused: a founder cannot kick themselves");
			return false;
		}
		if (RankIsFounder(org, targetCharacterId))
		{
			LF_Debug.Error("Organizations", "kick refused: the founder stays");
			return false;
		}
		if (!org.Outranks(actorCharacterId, targetCharacterId))
		{
			LF_Debug.Error("Organizations", string.Format("kick refused: %1 does not outrank %2", actorCharacterId, targetCharacterId));
			return false;
		}
		if (!org.RemoveMember(targetCharacterId))
		{
			LF_Debug.Error("Organizations", string.Format("kick refused: %1 was not removed", targetCharacterId));
			return false;
		}

		GetInstance().m_mMembership.Remove(targetCharacterId);
		LF_Debug.Log("Organizations", string.Format("kick org=%1 member=%2", orgId, targetCharacterId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One rung up for a member below the actor. The actor needs MANAGE_RANKS and must stay above the
	//! new rank: nobody promotes to their own rung or past it, so the founder rung is never given.
	static bool Promote(string orgId, string actorCharacterId, string targetCharacterId)
	{
		return MoveRank(orgId, actorCharacterId, targetCharacterId, -1, "promote");
	}

	//------------------------------------------------------------------------------------------------
	//! One rung down for a member below the actor. The actor needs MANAGE_RANKS.
	static bool Demote(string orgId, string actorCharacterId, string targetCharacterId)
	{
		return MoveRank(orgId, actorCharacterId, targetCharacterId, 1, "demote");
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] step -1 one rung up, 1 one rung down.
	protected static bool MoveRank(string orgId, string actorCharacterId, string targetCharacterId, int step, string verb)
	{
		if (!Server())
			return false;

		LF_Organization org = Get(orgId);
		if (!org)
		{
			LF_Debug.Error("Organizations", string.Format("%1 refused: no organization %2", verb, orgId));
			return false;
		}
		if (actorCharacterId.IsEmpty() || actorCharacterId == targetCharacterId)
		{
			LF_Debug.Error("Organizations", string.Format("%1 refused: %2 cannot change their own rank", verb, actorCharacterId));
			return false;
		}
		if (!org.IsMember(targetCharacterId))
		{
			LF_Debug.Error("Organizations", string.Format("%1 refused: %2 is not a member", verb, targetCharacterId));
			return false;
		}
		if (!org.Allows(actorCharacterId, LF_EOrganizationPermission.MANAGE_RANKS))
		{
			LF_Debug.Error("Organizations", string.Format("%1 refused: %2 lacks the manage-ranks right", verb, actorCharacterId));
			return false;
		}
		if (!org.Outranks(actorCharacterId, targetCharacterId))
		{
			LF_Debug.Error("Organizations", string.Format("%1 refused: %2 does not outrank %3", verb, actorCharacterId, targetCharacterId));
			return false;
		}

		int position = org.RankPosition(targetCharacterId) + step;
		if (position <= org.RankPosition(actorCharacterId))
		{
			LF_Debug.Error("Organizations", string.Format("%1 refused: %2 would reach the rank of %3", verb, targetCharacterId, actorCharacterId));
			return false;
		}

		string rankId = org.RankIdAt(position);
		if (rankId.IsEmpty())
		{
			LF_Debug.Error("Organizations", string.Format("%1 refused: %2 is already on the last rung", verb, targetCharacterId));
			return false;
		}
		if (!org.SetMemberRank(targetCharacterId, rankId))
		{
			LF_Debug.Error("Organizations", string.Format("%1 refused: %2 was not moved", verb, targetCharacterId));
			return false;
		}

		LF_Debug.Info("Organizations", string.Format("%1 org=%2 actor=%3 member=%4 rank=%5 rights=%6", verb, orgId, actorCharacterId, targetCharacterId, rankId, LF_OrgRights.Describe(org.RightsOf(targetCharacterId))));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool Leave(string characterId)
	{
		if (!Server())
			return false;

		LF_Organization org = GetByCharacter(characterId);
		if (!org)
		{
			LF_Debug.Error("Organizations", string.Format("leave refused: %1 has no organization", characterId));
			return false;
		}
		if (RankIsFounder(org, characterId))
		{
			LF_Debug.Error("Organizations", "leave refused: the founder stays");
			return false;
		}
		if (!org.RemoveMember(characterId))
		{
			LF_Debug.Error("Organizations", string.Format("leave refused: %1 was not removed", characterId));
			return false;
		}

		GetInstance().m_mMembership.Remove(characterId);
		LF_Debug.Log("Organizations", string.Format("leave org=%1 member=%2", org.GetId(), characterId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool Deposit(string orgId, string characterId, int amount)
	{
		if (!Server())
			return false;
		if (!IsValidAmount(amount))
		{
			LF_Debug.Error("Organizations", string.Format("deposit refused: invalid amount %1", amount));
			return false;
		}

		LF_Organization org = Get(orgId);
		if (!org)
		{
			LF_Debug.Error("Organizations", string.Format("deposit refused: no organization %1", orgId));
			return false;
		}
		if (!org.IsMember(characterId))
		{
			LF_Debug.Error("Organizations", string.Format("deposit refused: %1 is not a member", characterId));
			return false;
		}

		org.Credit(amount);
		LF_Debug.Log("Organizations", string.Format("deposit org=%1 character=%2 amount=%3 fund=%4", orgId, characterId, amount, org.GetFund()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool Withdraw(string orgId, string characterId, int amount)
	{
		if (!Server())
			return false;
		if (!IsValidAmount(amount))
		{
			LF_Debug.Error("Organizations", string.Format("withdraw refused: invalid amount %1", amount));
			return false;
		}

		LF_Organization org = Get(orgId);
		if (!org)
		{
			LF_Debug.Error("Organizations", string.Format("withdraw refused: no organization %1", orgId));
			return false;
		}
		if (!org.Allows(characterId, LF_EOrganizationPermission.WITHDRAW))
		{
			LF_Debug.Error("Organizations", string.Format("withdraw refused: %1 lacks withdraw rank", characterId));
			return false;
		}
		if (!org.Debit(amount))
		{
			LF_Debug.Error("Organizations", string.Format("withdraw refused: fund %1 is short of %2", org.GetFund(), amount));
			return false;
		}

		LF_Debug.Log("Organizations", string.Format("withdraw org=%1 character=%2 amount=%3 fund=%4", orgId, characterId, amount, org.GetFund()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Pays the whole price from the fund or pays nothing. Does not touch a personal ATM balance.
	static bool BuyUpgrade(string orgId, string characterId, LF_EOrganizationUpgrade upgrade)
	{
		if (!Server())
			return false;

		LF_Organization org = Get(orgId);
		if (!org)
		{
			LF_Debug.Error("Organizations", string.Format("upgrade refused: no organization %1", orgId));
			return false;
		}

		string flag = UpgradeFlagName(upgrade);
		if (flag.IsEmpty())
		{
			LF_Debug.Error("Organizations", "upgrade refused: unknown upgrade");
			return false;
		}
		if (!org.Allows(characterId, LF_EOrganizationPermission.BUY_UPGRADE))
		{
			LF_Debug.Error("Organizations", string.Format("upgrade refused: %1 lacks buy rank", characterId));
			return false;
		}
		if (org.HasUpgrade(upgrade))
		{
			LF_Debug.Error("Organizations", string.Format("upgrade refused: %1 is already owned", flag));
			return false;
		}

		int price = UpgradePrice(upgrade);
		if (price <= 0)
		{
			LF_Debug.Error("Organizations", string.Format("upgrade refused: %1 has no price", flag));
			return false;
		}
		if (org.GetFund() < price)
		{
			LF_Debug.Error("Organizations", string.Format("upgrade refused: fund %1 is short of price %2", org.GetFund(), price));
			return false;
		}
		if (!org.Debit(price))
		{
			LF_Debug.Error("Organizations", string.Format("upgrade refused: price %1 was not taken", price));
			return false;
		}

		org.SetUpgrade(upgrade, true);
		LF_Debug.Log("Organizations", string.Format("upgrade bought org=%1 price=%2 flag=%3", orgId, price, flag));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void LF_OrganizationManager()
	{
		s_Instance = this;
	}

	//------------------------------------------------------------------------------------------------
	void ~LF_OrganizationManager()
	{
		s_Instance = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void IndexMembers(LF_Organization org)
	{
		if (!org)
			return;

		LF_OrganizationRecord snapshot = org.ToRecord();
		if (!snapshot || !snapshot.m_aMembers)
			return;

		foreach (LF_OrganizationMemberRecord member : snapshot.m_aMembers)
		{
			if (!member || member.m_sCharacterId.IsEmpty())
				continue;

			string existing = m_mMembership.Get(member.m_sCharacterId);
			if (!existing.IsEmpty() && existing != org.GetId())
			{
				org.RemoveMember(member.m_sCharacterId);
				LF_Debug.Error("Organizations", string.Format("load dropped %1 from %2: already in %3", member.m_sCharacterId, org.GetId(), existing));
				continue;
			}

			m_mMembership.Set(member.m_sCharacterId, org.GetId());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static bool RankIsFounder(LF_Organization org, string characterId)
	{
		if (!org)
			return false;

		return org.GetRankId(characterId) == RANK_FOUNDER;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Server()
	{
		if (Replication.IsServer())
			return true;

		LF_Debug.Error("Organizations", "rejected: not the server");
		return false;
	}
}
