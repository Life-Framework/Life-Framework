//------------------------------------------------------------------------------------------------
//! Who may place each kind (#2446, epic #927, refs #1030). Each placeable kind names a rule in
//! Configs/Placement/LF_PlacementPolicy.conf: anyone, on-duty police, the owner of (or a key holder
//! for) the house the player stands at, or a member of an organization at or above a minimum rank.
//! A kind with no entry keeps the rule its placeable or composition names (`m_eWho`), so a prefab
//! authored before the policy still works. LF_PlacementService and LF_PlacementCompositions ask
//! `Check`; the ghost's action gate asks it too (through MayPlace), so the answer is one rule on both
//! sides. An entry may also name a zone (`m_eWhere`, #2447; LF_PlacementZones) that limits where the
//! kind goes; the server's Confirm asks `ZoneAt` with the spot. An entry may name a cost (`m_eCost`, #2448;
//! LF_PlacementCost) the server's Confirm takes once the spot is VALID. A refusal names its rule in the log (`reason`) and in the toast (`toastKey`).
//! Ownership, keys and ranks are read from Houses and Organizations; this file edits neither.
//------------------------------------------------------------------------------------------------

//! One kind's rule in the policy `.conf`.
[BaseContainerProps()]
class LF_PlacementPolicyEntry
{
	[Attribute("", UIWidgets.EditBox, "The placeable kind: the placeable's own kind, else its placed prefab's file name (SpikeStrip), or a composition's name (roadblock)")]
	string m_sKind;

	[Attribute("0", UIWidgets.ComboBox, desc: "Who may place this kind", enums: ParamEnumArray.FromEnum(LF_EPlacementWho))]
	LF_EPlacementWho m_eWho;

	[Attribute("", UIWidgets.EditBox, "ORG_MEMBER only: the lowest rank id that may place it (founder, officer, member); empty lets any member")]
	string m_sMinRank;

	[Attribute("0", UIWidgets.ComboBox, desc: "Where this kind may go (#2447): anywhere, roads only, inside an owned or keyed property, or near the organization's base", enums: ParamEnumArray.FromEnum(LF_EPlacementWhere))]
	LF_EPlacementWhere m_eWhere;

	[Attribute("30", UIWidgets.EditBox, "ORG_BASE only: how many metres from the organization's base the kind may be placed")]
	float m_fBaseRadius;

	[Attribute("0", UIWidgets.ComboBox, desc: "What placing this kind costs (#2448): free, cash from the player's pockets, or items used up; taken on the server once the spot is valid", enums: ParamEnumArray.FromEnum(LF_EPlacementCost))]
	LF_EPlacementCost m_eCost;

	[Attribute("0", UIWidgets.EditBox, "CASH: the price; ITEM: how many items are used up. Zero or less is free")]
	int m_iCostAmount;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "ITEM only: the prefab used up from the player's inventory (never the placeable item itself)", "et")]
	ResourceName m_sCostItem;

	[Attribute("0", UIWidgets.CheckBox, "Whether picking the placement up gives the cost back, once, to the player who paid")]
	bool m_bRefundOnPickup;
}

//------------------------------------------------------------------------------------------------
//! The policy table, loaded from Configs/Placement/LF_PlacementPolicy.conf. A missing or broken file
//! logs once and every kind falls back to its own `m_eWho`.
[BaseContainerProps(configRoot: true)]
class LF_PlacementPolicyConfig
{
	static const ResourceName CONFIG_PATH = "{6C0E3B5A9D2F4871}Configs/Placement/LF_PlacementPolicy.conf";

	[Attribute("", UIWidgets.Object, "One entry per placeable kind")]
	ref array<ref LF_PlacementPolicyEntry> m_aEntries;

	[Attribute("40", UIWidgets.EditBox, "Most permanent placements one player keeps (#1032, LF_PlacementSave)")]
	int m_iOwnerCap;
	[Attribute("25", UIWidgets.EditBox, "Most permanent placements one property holds (#1032, LF_PlacementSave)")]
	int m_iPropertyCap;

	protected static ref LF_PlacementPolicyConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded table, or null (logged once) when the file does not resolve.
	static LF_PlacementPolicyConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Placement", string.Format("placement policy does not resolve, every kind keeps its own rule: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_PlacementPolicyConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("Placement", string.Format("placement policy has an unexpected root class, every kind keeps its own rule: %1", CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the table Get returns until Reload.
	static void UseForTest(LF_PlacementPolicyConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The entry for a kind, null when the table names none.
	LF_PlacementPolicyEntry Find(string kind)
	{
		if (!m_aEntries || kind.IsEmpty())
			return null;

		foreach (LF_PlacementPolicyEntry entry : m_aEntries)
		{
			if (entry && entry.m_sKind == kind)
				return entry;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! What the rules read about the player. `Judge` is pure over this; `Gather` fills it from the game.
class LF_PlacementWhoFacts
{
	bool m_bOnDuty;
	//! The house the player stands at, empty when none is within reach.
	string m_sHouseId;
	bool m_bHouseOwner;
	bool m_bKeyHolder;
	//! The player's organization, empty when they are in none.
	string m_sOrgId;
	//! The player's rank in it: ladder position, 0 is the top; -1 for no organization.
	int m_iRankPosition = -1;
	//! The entry's minimum rank id (empty: any member), and its ladder position in the player's
	//! organization (-1 when that organization has no such rank).
	string m_sMinRank;
	int m_iMinRankPosition = -1;
}

//------------------------------------------------------------------------------------------------
class LF_PlacementPolicy
{
	static const string FEATURE = "Placement";

	//! How far the player's feet may lie from a house for it to be "their house" (the building's origin).
	static const float HOUSE_RANGE = 30; // lf-fixed: geometry, how near a house's origin counts as at that house

	//! Toast keys, one per rule (the generic #LF-Placement_NotAllowed stays for a rule with no key).
	static const string TOAST_POLICE = "#LF-Placement_NeedPolice";
	static const string TOAST_HOUSE = "#LF-Placement_NeedHouse";
	static const string TOAST_ORG = "#LF-Placement_NeedOrg";
	static const string TOAST_RANK = "#LF-Placement_NeedRank";

	//------------------------------------------------------------------------------------------------
	//! The kind a placed prefab goes by when its placeable names none: the file name without folders or
	//! extension.
	static string KindOfPrefab(ResourceName prefab)
	{
		if (prefab.IsEmpty())
			return "";

		return FilePath.StripExtension(FilePath.StripPath(prefab));
	}

	//------------------------------------------------------------------------------------------------
	//! The rule for a kind: the table's entry, else the fallback the placeable or composition names.
	//! \param[out] minRank The entry's minimum rank id, empty when none.
	static LF_EPlacementWho RuleFor(string kind, LF_EPlacementWho fallback, out string minRank)
	{
		minRank = "";
		LF_PlacementPolicyConfig config = LF_PlacementPolicyConfig.Get();
		if (!config)
			return fallback;

		LF_PlacementPolicyEntry entry = config.Find(kind);
		if (!entry)
			return fallback;

		minRank = entry.m_sMinRank;
		return entry.m_eWho;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure. Whether the facts satisfy `who`. Returns empty when the player may place, else the refusal
	//! tail naming the rule ("is not on-duty police (rule ON_DUTY_POLICE)"); `toastKey` is that rule's toast.
	static string Judge(LF_EPlacementWho who, notnull LF_PlacementWhoFacts facts, out string toastKey)
	{
		toastKey = "";
		if (who == LF_EPlacementWho.ON_DUTY_POLICE)
		{
			if (facts.m_bOnDuty)
				return "";

			toastKey = TOAST_POLICE;
			return "is not on-duty police (rule ON_DUTY_POLICE)";
		}

		if (who == LF_EPlacementWho.HOUSE_OWNER_OR_KEY)
		{
			if (facts.m_sHouseId.IsEmpty())
			{
				toastKey = TOAST_HOUSE;
				return string.Format("has no house within %1 m (rule HOUSE_OWNER_OR_KEY)", HOUSE_RANGE);
			}

			if (facts.m_bHouseOwner || facts.m_bKeyHolder)
				return "";

			toastKey = TOAST_HOUSE;
			return string.Format("is neither the owner nor a key holder of house %1 (rule HOUSE_OWNER_OR_KEY)", facts.m_sHouseId);
		}

		if (who == LF_EPlacementWho.ORG_MEMBER)
		{
			if (facts.m_sOrgId.IsEmpty() || facts.m_iRankPosition < 0)
			{
				toastKey = TOAST_ORG;
				return "is not in an organization (rule ORG_MEMBER)";
			}

			if (facts.m_sMinRank.IsEmpty())
				return "";

			if (facts.m_iMinRankPosition < 0)
			{
				toastKey = TOAST_RANK;
				return string.Format("needs rank %1, which organization %2 does not have (rule ORG_MEMBER)", facts.m_sMinRank, facts.m_sOrgId);
			}

			if (facts.m_iRankPosition <= facts.m_iMinRankPosition)
				return "";

			toastKey = TOAST_RANK;
			return string.Format("ranks below %1 in organization %2 (rule ORG_MEMBER)", facts.m_sMinRank, facts.m_sOrgId);
		}

		// ANYONE, and any value this build does not know: the shipped default is open.
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Pure. Fills the organization facts from an organization and the player's character id.
	static void FillOrg(LF_Organization org, string characterId, string minRank, notnull LF_PlacementWhoFacts facts)
	{
		facts.m_sMinRank = minRank;
		if (!org || !org.IsMember(characterId))
			return;

		facts.m_sOrgId = org.GetId();
		facts.m_iRankPosition = org.RankPosition(characterId);
		if (minRank.IsEmpty())
			return;

		for (int i = 0; i < org.GetRankCount(); i++)
		{
			if (org.RankIdAt(i) == minRank)
				facts.m_iMinRankPosition = i;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Pure. Fills the house facts: the owner is the character whose key is the house's owner key; a key
	//! holder carries a key naming the house.
	static void FillHouse(string houseId, string ownerKey, string characterKey, notnull array<string> heldKeyHouseIds, notnull LF_PlacementWhoFacts facts)
	{
		facts.m_sHouseId = houseId;
		if (houseId.IsEmpty())
			return;

		if (!ownerKey.IsEmpty() && LF_VehicleLockComponent.IdentifiersMatch(characterKey, ownerKey))
			facts.m_bHouseOwner = true;

		foreach (string held : heldKeyHouseIds)
		{
			if (LF_VehicleLockComponent.IdentifiersMatch(held, houseId))
				facts.m_bKeyHolder = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The facts `who` reads, read from the game for `player`. Only what the rule needs is gathered, so
	//! a police gate costs no inventory walk.
	static LF_PlacementWhoFacts Gather(IEntity player, LF_EPlacementWho who, string minRank)
	{
		LF_PlacementWhoFacts facts = new LF_PlacementWhoFacts();
		if (!player)
			return facts;

		if (who == LF_EPlacementWho.ON_DUTY_POLICE)
			facts.m_bOnDuty = LF_PoliceUtils.IsOnDutyCharacter(player);
		else if (who == LF_EPlacementWho.HOUSE_OWNER_OR_KEY)
			GatherHouse(player, facts);
		else if (who == LF_EPlacementWho.ORG_MEMBER)
			GatherOrg(player, minRank, facts);

		return facts;
	}

	//------------------------------------------------------------------------------------------------
	protected static void GatherOrg(IEntity player, string minRank, notnull LF_PlacementWhoFacts facts)
	{
		string characterId = LF_OrganizationPlayer.CharacterId(player);
		if (characterId.IsEmpty())
			return;

		LF_OrganizationManager manager = LF_OrganizationManager.GetInstance();
		if (!manager)
			return;

		FillOrg(manager.GetByCharacter(characterId), characterId, minRank, facts);
	}

	//------------------------------------------------------------------------------------------------
	protected static void GatherHouse(IEntity player, notnull LF_PlacementWhoFacts facts)
	{
		// A player standing in the cottage they built is at that plot (#3173); the plot facts replace the
		// house ones, on the server from LF_Properties, on a client from the replicated mirror.
		array<string> plotKeys = {};
		HeldKeyHouseIds(player, plotKeys);
		if (LF_PlacementZones.GatherPlot(player.GetOrigin(), LF_Houses.CharacterKeyOf(player), plotKeys, facts))
			return;

		// The live-house table is filled on the server only (LF_Houses.Register), so a remote client reads
		// the houses around it and what each replicates (#3361).
		if (!Replication.IsServer())
		{
			GatherHouseClient(player, facts);
			return;
		}

		vector origin = player.GetOrigin();
		LF_HouseManagerComponent nearest;
		float nearestDistance = HOUSE_RANGE;
		map<string, LF_HouseManagerComponent> live = LF_HouseManagerComponent.Live();
		foreach (string id, LF_HouseManagerComponent house : live)
		{
			if (!house || !house.GetOwner())
				continue;

			float distance = vector.Distance(origin, house.GetOwner().GetOrigin());
			if (distance <= nearestDistance)
			{
				nearest = house;
				nearestDistance = distance;
			}
		}

		if (!nearest)
			return;

		array<string> heldKeys = {};
		HeldKeyHouseIds(player, heldKeys);
		FillHouse(nearest.GetHouseIdentifier(), nearest.GetOwnerKey(), LF_Houses.CharacterKeyOf(player), heldKeys, facts);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the house facts from the house managers streamed in around the player and the identifier and
	//! owner key they replicate. Never reads the server's live table. It only decides whether the prompt shows;
	//! the server's Confirm re-checks with the live table.
	static void GatherHouseClient(IEntity player, notnull LF_PlacementWhoFacts facts)
	{
		string houseId;
		string ownerKey;
		if (!NearestReplicatedHouse(player.GetOrigin(), houseId, ownerKey))
			return;

		array<string> heldKeys = {};
		HeldKeyHouseIds(player, heldKeys);
		FillHouse(houseId, ownerKey, LF_Houses.CharacterKeyOf(player), heldKeys, facts);
	}

	//------------------------------------------------------------------------------------------------
	//! The client branch for a spot, character key and held key house ids (the tests pass these without a
	//! player). Same rule as GatherHouseClient.
	static void GatherHouseClientFor(vector at, string characterKey, notnull array<string> heldKeyHouseIds, notnull LF_PlacementWhoFacts facts)
	{
		string houseId;
		string ownerKey;
		if (NearestReplicatedHouse(at, houseId, ownerKey))
			FillHouse(houseId, ownerKey, characterKey, heldKeyHouseIds, facts);
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest house within HOUSE_RANGE of `at`, found by a world query rather than the live table, so it
	//! works where only replicated state exists.
	//! \param[out] houseId The house's replicated identifier.
	//! \param[out] ownerKey The house's replicated owner key, empty when unowned.
	//! \return True when a house with an identifier lies within reach.
	static bool NearestReplicatedHouse(vector at, out string houseId, out string ownerKey)
	{
		houseId = "";
		ownerKey = "";
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		LF_PlacementHouseQuery query = new LF_PlacementHouseQuery(at, HOUSE_RANGE);
		// ALL, not STATIC: an island house is a mesh-less LF_HouseMarker, which a STATIC query never returns
		// (playtest 2026-11-27 round 5: the owner was never offered furniture placement in their own house).
		world.QueryEntitiesBySphere(at, HOUSE_RANGE, query.Collect, null, EQueryEntitiesFlags.ALL);
		if (!query.m_Nearest)
			return false;

		houseId = query.m_Nearest.GetHouseIdentifier();
		ownerKey = query.m_Nearest.GetOwnerKey();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The house identifier on every key the player carries (inventory and the left-hand slot).
	static void HeldKeyHouseIds(IEntity player, notnull array<string> ids)
	{
		CharacterControllerComponent controller = LF_Component<CharacterControllerComponent>.Find(player);
		IEntity inHand;
		if (controller)
			inHand = controller.GetAttachedGadgetAtLeftHandSlot();

		if (inHand)
		{
			LF_KeyComponent handKey = LF_Component<LF_KeyComponent>.Find(inHand);
			if (handKey)
				ids.Insert(handKey.GetHouseIdentifier());
		}

		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(player);
		if (!inventory)
			return;

		array<IEntity> items = {};
		inventory.GetItems(items);
		foreach (IEntity item : items)
		{
			LF_KeyComponent key = LF_Component<LF_KeyComponent>.Find(item);
			if (key)
				ids.Insert(key.GetHouseIdentifier());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server and client: whether `player` may place a `kind`, and if not the refusal tail and toast.
	//! \param fallback The rule the placeable or composition itself names, used when the table has no entry.
	//! \return Empty when allowed, else the refusal tail (see Judge).
	static string Check(IEntity player, string kind, LF_EPlacementWho fallback, out string toastKey)
	{
		toastKey = "";
		string minRank;
		LF_EPlacementWho who = RuleFor(kind, fallback, minRank);
		if (who == LF_EPlacementWho.ANYONE)
			return "";

		return Judge(who, Gather(player, who, minRank), toastKey);
	}

	//------------------------------------------------------------------------------------------------
	//! The zone for a kind: the table's entry, else ANYWHERE.
	//! \param[out] baseRadius The entry's base radius (ORG_BASE).
	static LF_EPlacementWhere WhereFor(string kind, out float baseRadius)
	{
		baseRadius = 0;
		LF_PlacementPolicyConfig config = LF_PlacementPolicyConfig.Get();
		if (!config)
			return LF_EPlacementWhere.ANYWHERE;

		LF_PlacementPolicyEntry entry = config.Find(kind);
		if (!entry)
			return LF_EPlacementWhere.ANYWHERE;

		baseRadius = entry.m_fBaseRadius;
		return entry.m_eWhere;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the kind's zone allows `spot` for `player` (#2447). The server's Confirm asks it once the
	//! proposal is well formed, so no gather runs on a malformed spot.
	//! \return Empty when allowed, else the refusal tail (see LF_PlacementZones.Judge).
	static string ZoneAt(IEntity player, string kind, vector spot, out string toastKey)
	{
		toastKey = "";
		float baseRadius;
		LF_EPlacementWhere zone = WhereFor(kind, baseRadius);
		if (zone == LF_EPlacementWhere.ANYWHERE)
			return "";

		return LF_PlacementZones.Judge(zone, LF_PlacementZones.Gather(player, spot, zone, baseRadius), toastKey);
	}
}

//------------------------------------------------------------------------------------------------
//! Collects the nearest house manager with an identifier from a world query (#3361).
class LF_PlacementHouseQuery
{
	vector m_vAt;
	float m_fNearest;
	LF_HouseManagerComponent m_Nearest;

	//------------------------------------------------------------------------------------------------
	void LF_PlacementHouseQuery(vector at, float range)
	{
		m_vAt = at;
		m_fNearest = range;
	}

	//------------------------------------------------------------------------------------------------
	bool Collect(IEntity entity)
	{
		LF_HouseManagerComponent house = LF_Component<LF_HouseManagerComponent>.Find(entity);
		if (!house || house.GetHouseIdentifier().IsEmpty())
			return true;

		float distance = vector.Distance(m_vAt, entity.GetOrigin());
		if (distance <= m_fNearest)
		{
			m_Nearest = house;
			m_fNearest = distance;
		}

		return true;
	}
}
