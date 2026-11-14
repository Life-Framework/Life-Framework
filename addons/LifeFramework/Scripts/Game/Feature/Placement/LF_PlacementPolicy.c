//------------------------------------------------------------------------------------------------
//! Placement rules (#1030, docs/roadmap/building-foundation.md sections 2 and 4): who may place a
//! kind of thing, where, and what it costs. One `LF_PlacementPolicy` record per kind, by id, in
//! Configs/Placement/LF_PlacementPolicy.conf; a placeable names its record (LF_PlaceableComponent
//! m_sPolicy). Packs and the owner's `placement` section add, replace, patch or remove records
//! through the override model (#962, docs/config/override-model.md).
//!
//!   who    licence, job and rank, organisation right: the catalogue's outlet gate (#988,
//!          LF_ItemCatalogue.Check), so one rule reads the same everywhere
//!   where  zone rules the spot may not lie in (or must lie in), a registered place within a radius
//!          (the place registry, #961), and whether it lies on a road
//!   cost   cash from the player, or an item used up; taken by the server only after the spot is VALID
//!
//! The rules read facts (LF_BuyerFacts, LF_PlacementSpotFacts, LF_PlacementPayer), so a LOGIC test
//! answers with a stub and the server with the world. Every refusal names the policy and the rule.
//------------------------------------------------------------------------------------------------

//! Why a policy said no. ALLOWED lets the placement through.
enum LF_EPlacementPolicyVerdict
{
	ALLOWED,
	UNKNOWN_POLICY,
	NEEDS_LICENCE,
	NEEDS_JOB,
	NEEDS_RANK,
	NEEDS_ORG_RIGHT,
	ZONE,
	PLACE,
	ROAD,
	CANNOT_PAY
}

//------------------------------------------------------------------------------------------------
//! One kind's rule. The core file ships neutral defaults (free, anywhere, anyone) plus the spike
//! strip; a pack sets prices and limits by patching a record.
[BaseContainerProps()]
class LF_PlacementPolicy : LF_ConfigRecord
{
	static const string ROAD_ANY = "";
	static const string ROAD_ON = "on";
	static const string ROAD_OFF = "off";

	[Attribute("", UIWidgets.EditBox, "Policy id a placeable names (LF_PlaceableComponent m_sPolicy): spike_strip")]
	string m_sId;

	[Attribute("", UIWidgets.EditBox, "Who: licence the placer must hold (LF_ELicenseType name). Empty is no licence gate")]
	string m_sLicence;

	[Attribute("", UIWidgets.EditBox, "Who: job the placer must be in (POLICE is an on-duty officer and the rank is the police rank; any other id is an LF_EJobType name the placer works now and the rank is the job level). Empty is no job gate")]
	string m_sJob;

	[Attribute("0", UIWidgets.Auto, "Who, with the job: the lowest rank (police rank index, or job level) that may place", params: "0 100")]
	int m_iMinRank;

	[Attribute("", UIWidgets.EditBox, "Who: organisation right the placer's rank must carry (LF_EOrganizationPermission name). Empty is no right gate")]
	string m_sOrgRight;

	[Attribute("", UIWidgets.EditBox, "Where: zone rules the spot may not lie in, comma separated (SAFE, LAWLESS, NORMAL; the zone rule sets). Empty is no limit")]
	string m_sBlockedZones;

	[Attribute("", UIWidgets.EditBox, "Where: the zone rule the spot must lie in (SAFE, LAWLESS, NORMAL). Empty is any zone")]
	string m_sOnlyZone;

	[Attribute("", UIWidgets.EditBox, "Where: place kind (the place registry, #961: house, org_base) the spot must lie near. Empty is anywhere")]
	string m_sPlaceKind;

	[Attribute("25", UIWidgets.Auto, "Where, with the place kind: metres from a place of that kind", params: "0 1000")]
	float m_fPlaceRadius;

	[Attribute("", UIWidgets.EditBox, "Where: 'on' lies on a road only, 'off' never on a road. Empty is either")]
	string m_sRoad;

	[Attribute("0", UIWidgets.Auto, "Cost: cash taken from the player after the spot is valid", params: "0 1000000")]
	int m_iCash;

	[Attribute("", UIWidgets.ResourceNamePicker, "Cost: item the player must carry and that is used up", "et")]
	ResourceName m_CostItem;

	[Attribute("1", UIWidgets.Auto, "Cost: how many of the item", params: "1 100")]
	int m_iCostItemCount;

	[Attribute("0", UIWidgets.CheckBox, "Cost: given back when the placed thing is picked up (#1032)")]
	bool m_bRefundOnPickup;

	//------------------------------------------------------------------------------------------------
	static LF_PlacementPolicy Make(string id)
	{
		LF_PlacementPolicy policy = new LF_PlacementPolicy();
		policy.m_sId = id;
		policy.m_fPlaceRadius = 25;
		policy.m_iCostItemCount = 1;
		return policy;
	}

	//------------------------------------------------------------------------------------------------
	//! The who gates as the catalogue's outlet gate, so LF_ItemCatalogue.Check judges them.
	LF_OutletOffer AsGate()
	{
		LF_OutletOffer gate = LF_OutletOffer.Make("placement", m_sId);
		gate.m_sLicence = m_sLicence;
		gate.m_sJob = m_sJob;
		gate.m_iMinRank = m_iMinRank;
		gate.m_sOrgRight = m_sOrgRight;
		return gate;
	}
}

//------------------------------------------------------------------------------------------------
//! What the where-rules ask about a spot. The world answers on the server; a LOGIC test stubs it.
class LF_PlacementSpotFacts
{
	//! The zone rule in force at the spot.
	LF_ZoneRule ZoneRule()
	{
		return LF_ZoneRule.NORMAL;
	}

	//! Metres from the spot to the nearest registered place of this kind; -1 when none is placed.
	float NearestPlace(string kind)
	{
		return -1;
	}

	//! Whether the spot lies on a road.
	bool OnRoad()
	{
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! The spot in the world: the zone registry, the place registry and the AI road network.
class LF_WorldSpotFacts : LF_PlacementSpotFacts
{
	protected vector m_vSpot;

	//------------------------------------------------------------------------------------------------
	static LF_WorldSpotFacts At(vector spot)
	{
		LF_WorldSpotFacts facts = new LF_WorldSpotFacts();
		facts.m_vSpot = spot;
		return facts;
	}

	//------------------------------------------------------------------------------------------------
	override LF_ZoneRule ZoneRule()
	{
		return LF_Zones.RuleAt(m_vSpot);
	}

	//------------------------------------------------------------------------------------------------
	override float NearestPlace(string kind)
	{
		array<IEntity> places = {};
		LF_Places.FindAll(kind, places);
		float best = -1;
		foreach (IEntity place : places)
		{
			float distance = vector.Distance(place.GetOrigin(), m_vSpot);
			if (best < 0 || distance < best)
				best = distance;
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnRoad()
	{
		vector roadDir;
		return LF_PlacementService.NearestRoad(m_vSpot, roadDir);
	}
}

//------------------------------------------------------------------------------------------------
//! What the cost asks about the placer's pockets.
class LF_PlacementPayer
{
	int Cash()
	{
		return 0;
	}

	int ItemCount(ResourceName prefab)
	{
		return 0;
	}
}

//------------------------------------------------------------------------------------------------
class LF_EntityPayer : LF_PlacementPayer
{
	protected IEntity m_Player;

	//------------------------------------------------------------------------------------------------
	static LF_EntityPayer Of(IEntity player)
	{
		LF_EntityPayer payer = new LF_EntityPayer();
		payer.m_Player = player;
		return payer;
	}

	//------------------------------------------------------------------------------------------------
	override int Cash()
	{
		return LF_MoneyUtils.GetCash(m_Player);
	}

	//------------------------------------------------------------------------------------------------
	override int ItemCount(ResourceName prefab)
	{
		return LF_InventoryUtils.GetAmount(m_Player, prefab);
	}
}

//------------------------------------------------------------------------------------------------
//! Who is asking, for a placement. The server reads the account; a client (which only hides an
//! action the server would refuse) knows whether the officer is on duty but not their rank, licences
//! or organisation, so it lets those gates through.
class LF_PlacementBuyerFacts : LF_EntityBuyerFacts
{
	protected bool m_bHint;

	//------------------------------------------------------------------------------------------------
	static LF_PlacementBuyerFacts For(IEntity user)
	{
		LF_PlacementBuyerFacts facts = new LF_PlacementBuyerFacts();
		facts.m_Buyer = user;
		facts.m_bHint = !Replication.IsServer();
		return facts;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLicence(LF_ELicenseType licence)
	{
		if (m_bHint)
			return true;

		return super.HasLicence(licence);
	}

	//------------------------------------------------------------------------------------------------
	override int JobRank(string job)
	{
		if (job != JOB_POLICE)
			return super.JobRank(job);

		// The same on-duty answer the placement service gave before the policy (#1026).
		if (!LF_PoliceUtils.IsOnDutyCharacter(m_Buyer))
			return -1;

		if (m_bHint)
			return LF_ItemCatalogueConfig.RANK_MAX;

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
			return -1;

		LF_PlayerAccount account = accounts.GetAccount(LF_PoliceRanks.ResolveUid(m_Buyer));
		if (!account)
			return -1;

		return account.GetPoliceRank();
	}

	//------------------------------------------------------------------------------------------------
	override int OrgRights()
	{
		if (m_bHint)
			return -1;

		return super.OrgRights();
	}
}

//------------------------------------------------------------------------------------------------
//! The rules themselves: pure, over facts. LF_PlacementService calls them on the server.
class LF_PlacementPolicies
{
	static const string FEATURE = "Placement";

	protected static ref set<string> s_aReported;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aReported = null;
		LF_PlacementPolicyConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	//! The policy a placeable names, or null when it names none (anyone may place it for free) or the
	//! id is unknown (logged; `known` is false then, and the placement is refused).
	static LF_PlacementPolicy Find(string id, out bool known)
	{
		known = true;
		if (id.IsEmpty())
			return null;

		LF_PlacementPolicyConfig config = LF_PlacementPolicyConfig.Get();
		LF_PlacementPolicy policy;
		if (config)
			policy = config.Find(id);

		if (!policy)
		{
			known = false;
			// Once per id: a client asks every frame an action is in view.
			if (!s_aReported)
				s_aReported = new set<string>();
			if (s_aReported.Find(id) < 0)
			{
				s_aReported.Insert(id);
				LF_Debug.Error(FEATURE, string.Format("placement policy '%1' does not exist; refusing", id));
			}
		}

		return policy;
	}

	//------------------------------------------------------------------------------------------------
	//! Who: the catalogue's gate over the policy's licence, job, rank and organisation right.
	static LF_EPlacementPolicyVerdict CheckWho(LF_PlacementPolicy policy, notnull LF_BuyerFacts who, out string reason)
	{
		reason = string.Empty;
		if (!policy)
			return LF_EPlacementPolicyVerdict.ALLOWED;

		string missing;
		LF_EOutletGate gate = LF_ItemCatalogue.Check(policy.AsGate(), who, missing);
		if (gate == LF_EOutletGate.OPEN)
			return LF_EPlacementPolicyVerdict.ALLOWED;

		if (gate == LF_EOutletGate.NEEDS_LICENCE)
		{
			reason = string.Format("policy %1: needs licence %2", policy.m_sId, policy.m_sLicence);
			return LF_EPlacementPolicyVerdict.NEEDS_LICENCE;
		}

		if (gate == LF_EOutletGate.NEEDS_JOB)
		{
			reason = string.Format("policy %1: needs job %2", policy.m_sId, policy.m_sJob);
			return LF_EPlacementPolicyVerdict.NEEDS_JOB;
		}

		if (gate == LF_EOutletGate.NEEDS_RANK)
		{
			reason = string.Format("policy %1: needs rank %2 in %3", policy.m_sId, policy.m_iMinRank, policy.m_sJob);
			return LF_EPlacementPolicyVerdict.NEEDS_RANK;
		}

		reason = string.Format("policy %1: needs organisation right %2", policy.m_sId, policy.m_sOrgRight);
		return LF_EPlacementPolicyVerdict.NEEDS_ORG_RIGHT;
	}

	//------------------------------------------------------------------------------------------------
	//! Where: the spot's zone rule, the place registry and the road.
	static LF_EPlacementPolicyVerdict CheckWhere(LF_PlacementPolicy policy, notnull LF_PlacementSpotFacts spot, out string reason)
	{
		reason = string.Empty;
		if (!policy)
			return LF_EPlacementPolicyVerdict.ALLOWED;

		string zone = typename.EnumToString(LF_ZoneRule, spot.ZoneRule());
		if (!policy.m_sOnlyZone.IsEmpty() && policy.m_sOnlyZone != zone)
		{
			reason = string.Format("policy %1: only in a %2 zone (this is %3)", policy.m_sId, policy.m_sOnlyZone, zone);
			return LF_EPlacementPolicyVerdict.ZONE;
		}

		array<string> blocked = {};
		policy.m_sBlockedZones.Split(",", blocked, true);
		foreach (string rule : blocked)
		{
			rule.Trim();
			if (rule == zone)
			{
				reason = string.Format("policy %1: not in a %2 zone", policy.m_sId, zone);
				return LF_EPlacementPolicyVerdict.ZONE;
			}
		}

		if (!policy.m_sPlaceKind.IsEmpty())
		{
			float distance = spot.NearestPlace(policy.m_sPlaceKind);
			if (distance < 0 || distance > policy.m_fPlaceRadius)
			{
				reason = string.Format("policy %1: not within %2 m of a %3 (nearest %4 m)", policy.m_sId, policy.m_fPlaceRadius, policy.m_sPlaceKind, distance);
				return LF_EPlacementPolicyVerdict.PLACE;
			}
		}

		if (policy.m_sRoad == LF_PlacementPolicy.ROAD_ON && !spot.OnRoad())
		{
			reason = string.Format("policy %1: only on a road", policy.m_sId);
			return LF_EPlacementPolicyVerdict.ROAD;
		}

		if (policy.m_sRoad == LF_PlacementPolicy.ROAD_OFF && spot.OnRoad())
		{
			reason = string.Format("policy %1: not on a road", policy.m_sId);
			return LF_EPlacementPolicyVerdict.ROAD;
		}

		return LF_EPlacementPolicyVerdict.ALLOWED;
	}

	//------------------------------------------------------------------------------------------------
	//! Cost: whether the placer holds the cash and the item.
	static LF_EPlacementPolicyVerdict CheckCost(LF_PlacementPolicy policy, notnull LF_PlacementPayer payer, out string reason)
	{
		reason = string.Empty;
		if (!policy)
			return LF_EPlacementPolicyVerdict.ALLOWED;

		if (policy.m_iCash > 0 && payer.Cash() < policy.m_iCash)
		{
			reason = string.Format("policy %1: costs %2 cash, the player has %3", policy.m_sId, policy.m_iCash, payer.Cash());
			return LF_EPlacementPolicyVerdict.CANNOT_PAY;
		}

		if (!policy.m_CostItem.IsEmpty() && payer.ItemCount(policy.m_CostItem) < policy.m_iCostItemCount)
		{
			reason = string.Format("policy %1: costs %2 x %3, the player has %4", policy.m_sId, policy.m_iCostItemCount, policy.m_CostItem, payer.ItemCount(policy.m_CostItem));
			return LF_EPlacementPolicyVerdict.CANNOT_PAY;
		}

		return LF_EPlacementPolicyVerdict.ALLOWED;
	}

	//------------------------------------------------------------------------------------------------
	//! All three, in order who, where, cost: the first that refuses is the answer.
	static LF_EPlacementPolicyVerdict Judge(LF_PlacementPolicy policy, notnull LF_BuyerFacts who, notnull LF_PlacementSpotFacts spot, notnull LF_PlacementPayer payer, out string reason)
	{
		LF_EPlacementPolicyVerdict verdict = CheckWho(policy, who, reason);
		if (verdict != LF_EPlacementPolicyVerdict.ALLOWED)
			return verdict;

		verdict = CheckWhere(policy, spot, reason);
		if (verdict != LF_EPlacementPolicyVerdict.ALLOWED)
			return verdict;

		return CheckCost(policy, payer, reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: takes the cost from the player. Call it once, after the spot is VALID and CheckCost
	//! passed. \return false (nothing taken) when the cash or the item cannot be taken in full.
	static bool Charge(LF_PlacementPolicy policy, IEntity player)
	{
		if (!policy || !player)
			return true;

		if (policy.m_iCash > 0 && LF_MoneyUtils.GetCash(player) < policy.m_iCash)
			return false;

		if (!policy.m_CostItem.IsEmpty() && LF_InventoryUtils.GetAmount(player, policy.m_CostItem) < policy.m_iCostItemCount)
			return false;

		if (policy.m_iCash > 0)
			LF_MoneyUtils.RemoveCash(player, policy.m_iCash, "placement");

		if (!policy.m_CostItem.IsEmpty())
			LF_InventoryUtils.RemoveAmount(player, policy.m_CostItem, policy.m_iCostItemCount);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: gives the cost back (a spawn that failed after the charge, a pickup of a placement
	//! whose policy says refundOnPickup is the caller's check).
	static void Refund(LF_PlacementPolicy policy, IEntity player)
	{
		if (!policy || !player)
			return;

		if (policy.m_iCash > 0)
			LF_MoneyUtils.AddCash(player, policy.m_iCash, "placement");

		if (!policy.m_CostItem.IsEmpty())
			LF_InventoryUtils.AddAmount(player, policy.m_CostItem, policy.m_iCostItemCount, true);
	}

	//------------------------------------------------------------------------------------------------
	//! The toast key for a refusal.
	static string ToastKey(LF_EPlacementPolicyVerdict verdict)
	{
		if (verdict == LF_EPlacementPolicyVerdict.ZONE)
			return "#LF-Placement_WrongZone";
		if (verdict == LF_EPlacementPolicyVerdict.PLACE)
			return "#LF-Placement_WrongPlace";
		if (verdict == LF_EPlacementPolicyVerdict.ROAD)
			return "#LF-Placement_RoadRule";
		if (verdict == LF_EPlacementPolicyVerdict.CANNOT_PAY)
			return "#LF-Placement_CannotPay";

		return "#LF-Placement_NotAllowed";
	}
}

//------------------------------------------------------------------------------------------------
//! The policy list: Configs/Placement/LF_PlacementPolicy.conf, then each pack overlay of this class,
//! then the owner's `placement` section. Every layer adds, replaces, patches or removes by policy
//! id (LF_ConfigOverlay, #962). Server rule; a client reads the same core and pack files to hide an
//! action the server would refuse.
[BaseContainerProps(configRoot: true)]
class LF_PlacementPolicyConfig
{
	static const ResourceName CONFIG_PATH = "{6E3A9B1D4C7F2085}Configs/Placement/LF_PlacementPolicy.conf";

	//! The owner config.json section this class reads (LF_ConfigService).
	static const string SECTION = "placement";

	static const int CASH_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int CASH_MAX = 1000000;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int RANK_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int RANK_MAX = 100;	// lf-fixed: the schema limit, mirrors the attribute params
	static const float RADIUS_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const float RADIUS_MAX = 1000;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int COUNT_MIN = 1;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int COUNT_MAX = 100;	// lf-fixed: the schema limit, mirrors the attribute params

	[Attribute("", UIWidgets.Object, "Placement policies, keyed by id: who may place a kind, where, and what it costs")]
	ref array<ref LF_PlacementPolicy> m_aPolicies;

	protected static ref LF_PlacementPolicyConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered policies, or null (logged once) when the core file does not resolve.
	static LF_PlacementPolicyConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(LF_PlacementPolicies.FEATURE, string.Format("placement policies do not resolve, a placeable with a policy is refused: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_PlacementPolicyConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
		{
			LF_Debug.Error(LF_PlacementPolicies.FEATURE, string.Format("placement policies have an unexpected root class: %1", CONFIG_PATH));
			return null;
		}

		s_Config.ApplyLayers();
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance so the next Get() layers the core file again (/configreload, world end).
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until Reload.
	static void UseForTest(LF_PlacementPolicyConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	LF_PlacementPolicy Find(string id)
	{
		if (!m_aPolicies)
			return null;

		foreach (LF_PlacementPolicy policy : m_aPolicies)
		{
			if (policy && policy.m_sId == id)
				return policy;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Layers each pack overlay of this class in load order, then the owner's section.
	void ApplyLayers()
	{
		if (!m_aPolicies)
			m_aPolicies = new array<ref LF_PlacementPolicy>();

		LF_PlacementPolicyList list = new LF_PlacementPolicyList(this);
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_PlacementPolicyConfig, SECTION, resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = LF_ConfigOverlay.ApplyContainerList(list, resource.GetResource().ToBaseContainer(), "m_aPolicies", names[i], claims);
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("placement: pack overlay %1 changed %2 policies", names[i], changed));
		}

		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (!reader)
			return;

		ApplyOwner(reader, LF_ConfigService.GetOwnerSource());
		LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("placement: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "placement" section: `policies` keyed by id, each with an optional "op".
	void ApplyOwner(notnull LF_ConfigReader reader, string source = "config.json")
	{
		array<string> ownerKeys = {"policies"};
		reader.WarnUnknownKeys(ownerKeys);
		if (!m_aPolicies)
			m_aPolicies = new array<ref LF_PlacementPolicy>();

		LF_ConfigOverlay.ApplyRecords(reader, "policies", new LF_PlacementPolicyList(this), source);
	}
}

//------------------------------------------------------------------------------------------------
//! The `policies` list for the shared merge, keyed by policy id.
class LF_PlacementPolicyList : LF_ConfigList
{
	protected LF_PlacementPolicyConfig m_Config;

	void LF_PlacementPolicyList(LF_PlacementPolicyConfig config)
	{
		m_Config = config;
		if (!m_Config.m_aPolicies)
			m_Config.m_aPolicies = new array<ref LF_PlacementPolicy>();
	}

	override string GetName()
	{
		return "placement.policies";
	}

	override int Count()
	{
		return m_Config.m_aPolicies.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aPolicies[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_PlacementPolicy.Cast(record).m_sId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sId", id);
		return id;
	}

	override string IdHint()
	{
		return "a policy id (spike_strip)";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		return LF_PlacementPolicy.Make(id);
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_PlacementPolicy policy = LF_PlacementPolicy.Cast(record);
		if (index < m_Config.m_aPolicies.Count())
			m_Config.m_aPolicies.Set(index, policy);
		else
			m_Config.m_aPolicies.Insert(policy);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aPolicies.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_PlacementPolicy policy = LF_PlacementPolicy.Cast(record);
		string costItem = policy.m_CostItem;
		int changed = LF_CatalogueFields.Copy(container, "m_sLicence", policy.m_sLicence);
		changed += LF_CatalogueFields.Copy(container, "m_sJob", policy.m_sJob);
		changed += LF_CatalogueFields.CopyInt(container, "m_iMinRank", policy.m_iMinRank);
		changed += LF_CatalogueFields.Copy(container, "m_sOrgRight", policy.m_sOrgRight);
		changed += LF_CatalogueFields.Copy(container, "m_sBlockedZones", policy.m_sBlockedZones);
		changed += LF_CatalogueFields.Copy(container, "m_sOnlyZone", policy.m_sOnlyZone);
		changed += LF_CatalogueFields.Copy(container, "m_sPlaceKind", policy.m_sPlaceKind);
		if (container.IsVariableSetDirectly("m_fPlaceRadius") && container.Get("m_fPlaceRadius", policy.m_fPlaceRadius))
			changed++;
		changed += LF_CatalogueFields.Copy(container, "m_sRoad", policy.m_sRoad);
		changed += LF_CatalogueFields.CopyInt(container, "m_iCash", policy.m_iCash);
		changed += LF_CatalogueFields.Copy(container, "m_CostItem", costItem);
		policy.m_CostItem = costItem;
		changed += LF_CatalogueFields.CopyInt(container, "m_iCostItemCount", policy.m_iCostItemCount);
		changed += LF_CatalogueFields.CopyBool(container, "m_bRefundOnPickup", policy.m_bRefundOnPickup);
		return changed;
	}

	override void PatchFromReader(notnull LF_ConfigRecord record, notnull LF_ConfigReader reader)
	{
		LF_PlacementPolicy policy = LF_PlacementPolicy.Cast(record);
		array<string> keys = {"op", "licence", "job", "minRank", "orgRight", "blockedZones", "onlyZone", "placeKind", "placeRadius", "road", "cash", "costItem", "costItemCount", "refundOnPickup"};
		reader.WarnUnknownKeys(keys);
		reader.ReadString("licence", policy.m_sLicence);
		reader.ReadString("job", policy.m_sJob);
		reader.ReadInt("minRank", policy.m_iMinRank, LF_PlacementPolicyConfig.RANK_MIN, LF_PlacementPolicyConfig.RANK_MAX);
		reader.ReadString("orgRight", policy.m_sOrgRight);
		reader.ReadString("blockedZones", policy.m_sBlockedZones);
		reader.ReadString("onlyZone", policy.m_sOnlyZone);
		reader.ReadString("placeKind", policy.m_sPlaceKind);
		reader.ReadFloat("placeRadius", policy.m_fPlaceRadius, LF_PlacementPolicyConfig.RADIUS_MIN, LF_PlacementPolicyConfig.RADIUS_MAX);
		string road = policy.m_sRoad;
		if (reader.ReadString("road", road))
		{
			if (road == LF_PlacementPolicy.ROAD_ANY || road == LF_PlacementPolicy.ROAD_ON || road == LF_PlacementPolicy.ROAD_OFF)
				policy.m_sRoad = road;
			else
				LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("%1.road = %2 is not 'on', 'off' or empty; keeping '%3'", reader.GetPath(), road, policy.m_sRoad));
		}

		reader.ReadInt("cash", policy.m_iCash, LF_PlacementPolicyConfig.CASH_MIN, LF_PlacementPolicyConfig.CASH_MAX);
		reader.ReadResource("costItem", policy.m_CostItem);
		reader.ReadInt("costItemCount", policy.m_iCostItemCount, LF_PlacementPolicyConfig.COUNT_MIN, LF_PlacementPolicyConfig.COUNT_MAX);
		reader.ReadBool("refundOnPickup", policy.m_bRefundOnPickup);
	}
}
