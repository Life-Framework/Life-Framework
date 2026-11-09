//------------------------------------------------------------------------------------------------
//! A persisted organization: stable id, name, ranks, members, a shared fund, and
//! upgrade flags. LNGC has no organization scripts (docs/roadmap/sources.md). The
//! fund follows LF_BankAccount: a balance keyed by organization id. It is not a
//! second currency and not a personal ATM balance. Cash enters and leaves only
//! through LF_OrganizationPlayer, which moves MoneyStack.
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

enum LF_EOrganizationPermission
{
	WITHDRAW,
	INVITE,
	KICK,
	BUY_UPGRADE
}

//------------------------------------------------------------------------------------------------
class LF_OrganizationConfig
{
	static const string RANK_FOUNDER = "founder";
	static const string RANK_MEMBER = "member";
}

//------------------------------------------------------------------------------------------------
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
//! Field order is the binary layout. Version is first.
class LF_OrganizationRecord
{
	static const int VERSION = 1;

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
	protected bool m_bCanWithdraw;
	protected bool m_bCanInvite;
	protected bool m_bCanKick;
	protected bool m_bCanBuyUpgrade;

	//------------------------------------------------------------------------------------------------
	string GetId()
	{
		return m_sId;
	}

	//------------------------------------------------------------------------------------------------
	bool Allows(LF_EOrganizationPermission permission)
	{
		if (permission == LF_EOrganizationPermission.WITHDRAW)
			return m_bCanWithdraw;
		if (permission == LF_EOrganizationPermission.INVITE)
			return m_bCanInvite;
		if (permission == LF_EOrganizationPermission.KICK)
			return m_bCanKick;
		if (permission == LF_EOrganizationPermission.BUY_UPGRADE)
			return m_bCanBuyUpgrade;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	LF_OrganizationRankRecord ToRecord()
	{
		return LF_OrganizationRankRecord.Create(m_sId, m_sName, m_bCanWithdraw, m_bCanInvite, m_bCanKick, m_bCanBuyUpgrade);
	}

	//------------------------------------------------------------------------------------------------
	static LF_OrganizationRank FromRecord(LF_OrganizationRankRecord record)
	{
		if (!record)
			return null;

		LF_OrganizationRank rank();
		rank.m_sId = record.m_sId;
		rank.m_sName = record.m_sName;
		rank.m_bCanWithdraw = record.m_bCanWithdraw;
		rank.m_bCanInvite = record.m_bCanInvite;
		rank.m_bCanKick = record.m_bCanKick;
		rank.m_bCanBuyUpgrade = record.m_bCanBuyUpgrade;
		return rank;
	}

	//------------------------------------------------------------------------------------------------
	static LF_OrganizationRank Create(string id, string name, bool canWithdraw, bool canInvite, bool canKick, bool canBuyUpgrade)
	{
		LF_OrganizationRank rank();
		rank.m_sId = id;
		rank.m_sName = name;
		rank.m_bCanWithdraw = canWithdraw;
		rank.m_bCanInvite = canInvite;
		rank.m_bCanKick = canKick;
		rank.m_bCanBuyUpgrade = canBuyUpgrade;
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
		org.m_aRanks.Insert(LF_OrganizationRank.Create(LF_OrganizationConfig.RANK_FOUNDER, "Founder", true, true, true, true));
		org.m_aRanks.Insert(LF_OrganizationRank.Create(LF_OrganizationConfig.RANK_MEMBER, "Member", false, false, false, false));
		org.AddMember(founderCharacterId, LF_OrganizationConfig.RANK_FOUNDER);
		return org;
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

		if (record.m_aRanks)
		{
			foreach (LF_OrganizationRankRecord rankRecord : record.m_aRanks)
			{
				LF_OrganizationRank rank = LF_OrganizationRank.FromRecord(rankRecord);
				if (rank && !rank.GetId().IsEmpty())
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
	static const int MAX_CASH_MOVE = 1000000;
	static const int PRICE_EXTRA_STORAGE = 500;
	static const int PRICE_PROCESSING_SLOT = 250;
	static const int PRICE_PARKED_VEHICLE = 750;
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

		return amount <= MAX_CASH_MOVE;
	}

	//------------------------------------------------------------------------------------------------
	static int UpgradePrice(LF_EOrganizationUpgrade upgrade)
	{
		if (upgrade == LF_EOrganizationUpgrade.EXTRA_STORAGE)
			return PRICE_EXTRA_STORAGE;
		if (upgrade == LF_EOrganizationUpgrade.PROCESSING_SLOT)
			return PRICE_PROCESSING_SLOT;
		if (upgrade == LF_EOrganizationUpgrade.PARKED_VEHICLE)
			return PRICE_PARKED_VEHICLE;

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
	static void ApplyAll(notnull array<ref LF_OrganizationRecord> records)
	{
		LF_OrganizationManager instance = GetInstance();
		instance.m_mOrganizations.Clear();
		instance.m_mMembership.Clear();

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

			instance.m_mOrganizations.Set(org.GetId(), org);
			instance.IndexMembers(org);
		}
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
		if (!org.AddMember(targetCharacterId, RANK_MEMBER))
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
