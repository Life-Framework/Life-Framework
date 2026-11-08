//------------------------------------------------------------------------------------------------
//! The registered identity printed on an ID card item (Prefabs/Items/Roleplay/IDCard.et).
//! Server-authoritative: only the server issues a card, once. A fresh card from the prefab is
//! blank until issued. The fields replicate as RplProps so the holder's own client can read the
//! card in its inventory (Show ID needs it).
//!
//! PERSISTENCE. The source of truth is the account's character record (name, age, id), which
//! is persisted. The card itself is session state: the town clerk hands the first one over
//! (arrival chain, M1-18), then EL_SpawnLogic grants a fresh one at every spawn
//! (LF_QuestManager.HasEarnedIdCard) from that record, so the card survives a restart.
//! If carried inventory comes back blank after a restart, LF_IdCard.Grant issues that blank card
//! instead of adding a second one.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/CharacterCreation", description: "Registered identity printed on an ID card.")]
class LF_IdCardComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_IdCardComponent : ScriptComponent
{
	[RplProp()]
	protected string m_sHolderId;

	[RplProp()]
	protected string m_sFirstName;

	[RplProp()]
	protected string m_sLastName;

	[RplProp()]
	protected int m_iAge;

	//------------------------------------------------------------------------------------------------
	//! Server: prints the identity on a blank card.
	//! \return False when this is not the server, the card is already issued, or no holder is given.
	bool Issue(string holderId, string firstName, string lastName, int age)
	{
		if (!Replication.IsServer())
			return false;

		if (holderId.IsEmpty())
		{
			EL_Debug.Error("IdCard", "card not issued: no holder id");
			return false;
		}

		if (IsIssued())
		{
			EL_Debug.Warn("IdCard", "card not issued: already printed for another holder");
			return false;
		}

		m_sHolderId = holderId;
		m_sFirstName = firstName;
		m_sLastName = lastName;
		m_iAge = age;
		Replication.BumpMe();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool IsIssued()
	{
		return !m_sHolderId.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The persistence id of the character the card was issued to, empty when blank.
	string GetHolderId()
	{
		return m_sHolderId;
	}

	//------------------------------------------------------------------------------------------------
	string GetFirstName()
	{
		return m_sFirstName;
	}

	//------------------------------------------------------------------------------------------------
	string GetLastName()
	{
		return m_sLastName;
	}

	//------------------------------------------------------------------------------------------------
	int GetAge()
	{
		return m_iAge;
	}
}

//------------------------------------------------------------------------------------------------
//! What a grant does given the cards a character already carries. Pure, for LOGIC tests.
enum LF_EIdCardGrant
{
	NEW,
	REISSUE_BLANK,
	REFUSED_HELD,
	REFUSED_NO_HOLDER
}

//------------------------------------------------------------------------------------------------
class LF_IdCardRules
{
	//------------------------------------------------------------------------------------------------
	//! \param holderId Character persistence id the card is for.
	//! \param carriedHolders Holder id of each card the character carries ("" for a blank card).
	static LF_EIdCardGrant Decide(string holderId, array<string> carriedHolders)
	{
		if (holderId.IsEmpty())
			return LF_EIdCardGrant.REFUSED_NO_HOLDER;

		if (!carriedHolders)
			return LF_EIdCardGrant.NEW;

		if (carriedHolders.Find(holderId) >= 0)
			return LF_EIdCardGrant.REFUSED_HELD;

		if (carriedHolders.Find(string.Empty) >= 0)
			return LF_EIdCardGrant.REISSUE_BLANK;

		return LF_EIdCardGrant.NEW;
	}

	//------------------------------------------------------------------------------------------------
	//! Which carried card a character shows: their own when they carry it, else the first one.
	//! \return Index into carriedHolders, or -1 when nothing printed is carried.
	static int PickShown(string ownHolderId, array<string> carriedHolders)
	{
		if (!carriedHolders)
			return -1;

		if (!ownHolderId.IsEmpty())
		{
			int own = carriedHolders.Find(ownHolderId);
			if (own >= 0)
				return own;
		}

		for (int i = 0; i < carriedHolders.Count(); i++)
		{
			if (!carriedHolders.Get(i).IsEmpty())
				return i;
		}

		return -1;
	}
}

//------------------------------------------------------------------------------------------------
//! Server helpers: find the cards a character carries and grant one.
class LF_IdCard
{
	static const ResourceName PREFAB = "{75459D9B0B635800}Prefabs/Items/Roleplay/IDCard.et";

	//------------------------------------------------------------------------------------------------
	//! Every ID card in the character's inventory or hands.
	static void FindCarried(IEntity character, notnull array<LF_IdCardComponent> outCards)
	{
		outCards.Clear();
		if (!character)
			return;

		array<IEntity> items = {};
		SCR_InventoryStorageManagerComponent inventory = EL_Component<SCR_InventoryStorageManagerComponent>.Find(character);
		if (inventory)
			inventory.GetItems(items);

		CharacterControllerComponent controller = EL_Component<CharacterControllerComponent>.Find(character);
		if (controller)
		{
			IEntity inHand = controller.GetAttachedGadgetAtLeftHandSlot();
			if (inHand && items.Find(inHand) < 0)
				items.Insert(inHand);
		}

		foreach (IEntity item : items)
		{
			if (!item)
				continue;

			// GetItems can list one item more than once; a card counts once.
			LF_IdCardComponent card = LF_IdCardComponent.Cast(item.FindComponent(LF_IdCardComponent));
			if (card && outCards.Find(card) < 0)
				outCards.Insert(card);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void HolderIds(array<LF_IdCardComponent> cards, notnull array<string> outHolders)
	{
		outHolders.Clear();
		if (!cards)
			return;

		foreach (LF_IdCardComponent card : cards)
		{
			outHolders.Insert(card.GetHolderId());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The card a character holds up when showing ID, or null when they carry no printed card.
	static LF_IdCardComponent FindShown(IEntity character, string ownHolderId)
	{
		array<LF_IdCardComponent> cards = {};
		FindCarried(character, cards);
		array<string> holders = {};
		HolderIds(cards, holders);

		int index = LF_IdCardRules.PickShown(ownHolderId, holders);
		if (index < 0)
			return null;

		return cards.Get(index);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: gives the character a card printed with this identity. A character that already
	//! carries their own card is refused; a blank carried card is printed instead of adding one.
	//! \return True when the character now carries a newly printed card.
	static bool Grant(IEntity character, string holderId, string firstName, string lastName, int age)
	{
		if (!Replication.IsServer())
		{
			EL_Debug.Error("IdCard", "grant refused: not the server");
			return false;
		}

		if (!character)
		{
			EL_Debug.Error("IdCard", "grant refused: no character");
			return false;
		}

		array<LF_IdCardComponent> cards = {};
		FindCarried(character, cards);
		array<string> holders = {};
		HolderIds(cards, holders);

		LF_EIdCardGrant decision = LF_IdCardRules.Decide(holderId, holders);
		if (decision == LF_EIdCardGrant.REFUSED_NO_HOLDER)
		{
			EL_Debug.Error("IdCard", "grant refused: no holder id");
			return false;
		}

		if (decision == LF_EIdCardGrant.REFUSED_HELD)
		{
			EL_Debug.Warn("IdCard", "grant refused: character " + holderId + " already carries their card");
			return false;
		}

		if (decision == LF_EIdCardGrant.REISSUE_BLANK)
		{
			LF_IdCardComponent blank = cards.Get(holders.Find(string.Empty));
			if (!blank.Issue(holderId, firstName, lastName, age))
				return false;

			EL_Debug.Info("IdCard", "blank carried card printed for character " + holderId);
			return true;
		}

		InventoryStorageManagerComponent storage = EL_Component<InventoryStorageManagerComponent>.Find(character);
		if (!storage)
		{
			EL_Debug.Error("IdCard", "card not granted: character has no inventory");
			return false;
		}

		IEntity item = EL_Utils.SpawnEntityPrefab(PREFAB, character.GetOrigin());
		if (!item)
			return false;

		LF_IdCardComponent card = LF_IdCardComponent.Cast(item.FindComponent(LF_IdCardComponent));
		if (!card)
		{
			EL_Debug.Error("IdCard", "card not granted: " + PREFAB + " has no LF_IdCardComponent");
			SCR_EntityHelper.DeleteEntityAndChildren(item);
			return false;
		}

		card.Issue(holderId, firstName, lastName, age);
		if (!storage.TryInsertItem(item))
		{
			EL_Debug.Error("IdCard", "card not granted: no room in the inventory of character " + holderId);
			SCR_EntityHelper.DeleteEntityAndChildren(item);
			return false;
		}

		EL_Debug.Info("IdCard", "card granted to character " + holderId);
		return true;
	}
}
