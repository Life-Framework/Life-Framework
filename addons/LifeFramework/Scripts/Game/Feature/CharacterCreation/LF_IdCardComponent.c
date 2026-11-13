//------------------------------------------------------------------------------------------------
//! The registered identity printed on an ID card item (Prefabs/Items/Roleplay/IDCard.et).
//! Server-authoritative: only the server issues a card, once. A fresh card from the prefab is
//! blank until issued. The fields replicate as RplProps so the holder's own client can read the
//! card in its inventory (Show ID needs it).
//!
//! PERSISTENCE (#902). Every printed card is a row in the registry (LF_PhotoIdCard, the "idcards"
//! payload): number, holder, name, age, date of issue and photo. The card item carries only its
//! number, in its LF_SerialComponent, which persists with the item wherever it lies. A card
//! restored after a restart comes back with its number and nothing printed; LF_IdCard.EnsurePrinted
//! prints it from the row (at spawn, and whenever the server reads a card). The town clerk hands
//! the first card over (arrival chain, M1-18) and replaces a lost one for a fee.
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
			LF_Debug.Error("IdCard", "card not issued: no holder id");
			return false;
		}

		if (IsIssued())
		{
			LF_Debug.Warn("IdCard", "card not issued: already printed for another holder");
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
//! Server helpers: find the cards a character carries, grant one, and print one back after a
//! restart. The card's number is its LF_SerialComponent; the registry row behind the number
//! (LF_PhotoIdCard) holds what is printed and the photo.
class LF_IdCard
{
	static const ResourceName PREFAB = "{75459D9B0B635800}Prefabs/Items/Roleplay/IDCard.et";
	//! Stands for a void (replaced) card in the holder list LF_IdCardRules reads: it is nobody's
	//! card any more, and it is not blank either.
	static const string VOID_HOLDER = "#void";

	//------------------------------------------------------------------------------------------------
	//! Every ID card in the character's inventory or hands.
	static void FindCarried(IEntity character, notnull array<LF_IdCardComponent> outCards)
	{
		outCards.Clear();
		if (!character)
			return;

		array<IEntity> items = {};
		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(character);
		if (inventory)
			inventory.GetItems(items);

		CharacterControllerComponent controller = LF_Component<CharacterControllerComponent>.Find(character);
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
	//! The number printed on the card, empty for a card from before the registry (#902).
	static string NumberOf(LF_IdCardComponent card)
	{
		if (!card)
			return string.Empty;

		LF_SerialComponent serial = LF_SerialComponent.Cast(card.GetOwner().FindComponent(LF_SerialComponent));
		if (!serial)
			return string.Empty;

		return serial.GetSerial();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a card replaced since it was printed is void. A printed card with no number (made
	//! before the registry) counts as valid; a blank card is not.
	static bool IsValid(LF_IdCardComponent card)
	{
		if (!card || !card.IsIssued())
			return false;

		string number = NumberOf(card);
		if (number.IsEmpty())
			return true;

		return LF_PhotoIdCard.IsValid(number);
	}

	//------------------------------------------------------------------------------------------------
	//! Holder of each card for LF_IdCardRules: "" for a blank card, VOID_HOLDER for a void one (the
	//! server knows; a client lists the printed holder). A client lists a numbered card that is not
	//! printed yet (restored after a restart) by its number: it is a card to show, and the server
	//! prints it from the registry before it reads it.
	static void HolderIds(array<LF_IdCardComponent> cards, notnull array<string> outHolders)
	{
		outHolders.Clear();
		if (!cards)
			return;

		bool server = Replication.IsServer();
		foreach (LF_IdCardComponent card : cards)
		{
			if (server && card.IsIssued() && !IsValid(card))
				outHolders.Insert(VOID_HOLDER);
			else if (!server && !card.IsIssued())
				outHolders.Insert(NumberOf(card));
			else
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
		if (index >= 0)
			return cards.Get(index);

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: prints a blank card that has a number, from the registry row (a card restored after a
	//! restart comes back with its number and nothing printed).
	//! \return True when the card is printed now.
	static bool EnsurePrinted(LF_IdCardComponent card)
	{
		if (!card || !Replication.IsServer())
			return false;

		if (card.IsIssued())
			return true;

		string number = NumberOf(card);
		if (number.IsEmpty())
			return false;

		LF_PhotoIdRow row = LF_PhotoIdCard.FindNumber(number);
		if (!row)
		{
			LF_Debug.Warn(LF_PhotoIdCard.FEATURE, string.Format("card %1 stays blank: the registry has no such number", number));
			return false;
		}

		if (!card.Issue(row.m_sHolderId, row.m_sFirstName, row.m_sLastName, row.m_iAge))
			return false;

		LF_Debug.Info(LF_PhotoIdCard.FEATURE, string.Format("card %1 printed again for character %2", number, row.m_sHolderId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: EnsurePrinted for every card the character carries. \return Cards printed now.
	static int RepairCarried(IEntity character)
	{
		array<LF_IdCardComponent> cards = {};
		FindCarried(character, cards);
		int printed;
		foreach (LF_IdCardComponent card : cards)
		{
			if (!card.IsIssued() && EnsurePrinted(card))
				printed++;
		}

		return printed;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: true when this exact card is in the character's inventory or hands (a client asking to
	//! read a card names it; it may only read one it carries).
	static bool IsOwnerCarrying(IEntity character, LF_IdCardComponent card)
	{
		if (!character || !card)
			return false;

		array<LF_IdCardComponent> carried = {};
		FindCarried(character, carried);
		return carried.Find(card) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: true when the character carries the holder's valid card.
	static bool CarriesValid(IEntity character, string holderId)
	{
		if (holderId.IsEmpty())
			return false;

		array<LF_IdCardComponent> cards = {};
		FindCarried(character, cards);
		foreach (LF_IdCardComponent card : cards)
		{
			if (card.GetHolderId() == holderId && IsValid(card))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: gives the character a newly printed card with this identity: a new number in the
	//! registry, the date of issue, and the photo taken of the character as they stand now. A
	//! character that already carries their valid card is refused; a blank carried card is printed
	//! instead of adding one.
	//! \return True when the character now carries a newly printed card.
	static bool Grant(IEntity character, string holderId, string firstName, string lastName, int age)
	{
		if (!Replication.IsServer())
		{
			LF_Debug.Error("IdCard", "grant refused: not the server");
			return false;
		}

		if (!character)
		{
			LF_Debug.Error("IdCard", "grant refused: no character");
			return false;
		}

		// A numbered card restored blank is someone's card: print it first, so it is never taken
		// for a blank and printed over with this identity.
		RepairCarried(character);
		array<LF_IdCardComponent> cards = {};
		FindCarried(character, cards);
		array<string> holders = {};
		HolderIds(cards, holders);

		LF_EIdCardGrant decision = LF_IdCardRules.Decide(holderId, holders);
		if (decision == LF_EIdCardGrant.REFUSED_NO_HOLDER)
		{
			LF_Debug.Error("IdCard", "grant refused: no holder id");
			return false;
		}

		if (decision == LF_EIdCardGrant.REFUSED_HELD)
		{
			LF_Debug.Warn("IdCard", "grant refused: character " + holderId + " already carries their card");
			return false;
		}

		if (decision == LF_EIdCardGrant.REISSUE_BLANK)
		{
			LF_IdCardComponent blank = cards.Get(holders.Find(string.Empty));
			if (!blank.Issue(holderId, firstName, lastName, age))
				return false;

			Number(blank, Record(character, holderId, firstName, lastName, age));
			LF_Debug.Info("IdCard", "blank carried card printed for character " + holderId);
			return true;
		}

		InventoryStorageManagerComponent storage = LF_Component<InventoryStorageManagerComponent>.Find(character);
		if (!storage)
		{
			LF_Debug.Error("IdCard", "card not granted: character has no inventory");
			return false;
		}

		IEntity item = LF_Utils.SpawnEntityPrefab(PREFAB, character.GetOrigin());
		if (!item)
			return false;

		LF_IdCardComponent card = LF_IdCardComponent.Cast(item.FindComponent(LF_IdCardComponent));
		if (!card)
		{
			LF_Debug.Error("IdCard", "card not granted: " + PREFAB + " has no LF_IdCardComponent");
			SCR_EntityHelper.DeleteEntityAndChildren(item);
			return false;
		}

		card.Issue(holderId, firstName, lastName, age);
		if (!storage.TryInsertItem(item))
		{
			LF_Debug.Error("IdCard", "card not granted: no room in the inventory of character " + holderId);
			SCR_EntityHelper.DeleteEntityAndChildren(item);
			return false;
		}

		Number(card, Record(character, holderId, firstName, lastName, age));
		LF_Debug.Info("IdCard", "card granted to character " + holderId);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A new registry row for the card being printed, with the photo taken now.
	protected static LF_PhotoIdRow Record(IEntity character, string holderId, string firstName, string lastName, int age)
	{
		return LF_PhotoIdCard.Issue(holderId, firstName, lastName, age, LF_PhotoIdCard.IssueDate(), LF_WantedLook.Capture(character));
	}

	//------------------------------------------------------------------------------------------------
	//! Stamps the row's number on the card (its LF_SerialComponent), over any old number.
	protected static void Number(LF_IdCardComponent card, LF_PhotoIdRow row)
	{
		if (!card || !row)
			return;

		LF_SerialComponent serial = LF_SerialComponent.Cast(card.GetOwner().FindComponent(LF_SerialComponent));
		if (!serial)
		{
			LF_Debug.Error(LF_PhotoIdCard.FEATURE, "card " + row.m_sNumber + " not numbered: " + PREFAB + " has no LF_SerialComponent; it prints blank after a restart");
			return;
		}

		serial.RestoreSerial(row.m_sNumber);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, on every spawn once the inventory is in place: prints the carried cards a restart
	//! left blank, then hands over the first card (a resident who earned it before the registry
	//! existed) or, in a mode with no clerk, a free replacement. A lost card otherwise stays lost:
	//! the town clerk replaces it for a fee (LF_DialogueHook_IdCardReplace).
	static LF_EIdSpawn OnSpawn(IEntity character, LF_PlayerAccount account, bool grantAtSpawn, bool earned)
	{
		RepairCarried(character);
		if (!earned && !grantAtSpawn)
			return LF_EIdSpawn.NOTHING;

		if (!account || !account.GetActiveCharacter())
		{
			LF_Debug.Error("Spawn", "ID card not granted: no account character for the spawned body");
			return LF_EIdSpawn.NOTHING;
		}

		LF_PlayerCharacter identity = account.GetActiveCharacter();
		string holderId = identity.GetId();
		bool carries = CarriesValid(character, holderId);
		LF_EIdSpawn decision = LF_PhotoIdCard.DecideSpawn(grantAtSpawn, LF_PhotoIdCard.Current(holderId) != null, carries);
		if (decision == LF_EIdSpawn.NOTHING)
		{
			if (!carries)
				LF_Debug.Info(LF_PhotoIdCard.FEATURE, string.Format("character %1 spawned without their card; the town clerk replaces it", holderId));

			return decision;
		}

		Grant(character, holderId, identity.GetFirstName(), identity.GetLastName(), identity.GetAge());
		return decision;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sends the card's print, its photo and whether it is void to a player's machine, which
	//! opens the card sheet (LF_IdCardView).
	//! \param shower The character holding the card up; null when the viewer reads their own.
	//! \param seconds How long the sheet stays open, 0 until closed.
	static bool SendView(int viewerId, LF_IdCardComponent card, IEntity shower, float seconds)
	{
		SCR_PlayerController viewer = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(viewerId));
		if (!viewer || !card)
		{
			LF_Debug.Warn(LF_PhotoIdCard.FEATURE, string.Format("card view not sent: player %1 has no controller or there is no card", viewerId));
			return false;
		}

		EnsurePrinted(card);
		string number = NumberOf(card);
		LF_PhotoIdRow row = LF_PhotoIdCard.FindNumber(number);
		string issued;
		string look;
		bool isVoid;
		if (row)
		{
			issued = row.m_sIssued;
			look = row.m_sLook;
			isVoid = !LF_PhotoIdCard.IsValid(number);
		}

		RplId showerId = RplId.Invalid();
		if (shower)
			showerId = LF_NetworkUtils.GetRplId(shower);

		viewer.LF_ShowIdCardView(card.GetFirstName(), card.GetLastName(), card.GetAge(), number, issued, look, isVoid, showerId, seconds);
		LF_Debug.Info(LF_PhotoIdCard.FEATURE, string.Format("card %1 view sent to player %2 (void %3, look %4 chars)", number, viewerId, isVoid, look.Length()));
		return true;
	}
}
