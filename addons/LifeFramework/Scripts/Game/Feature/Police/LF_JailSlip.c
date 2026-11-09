//------------------------------------------------------------------------------------------------
//! The sentence slip: a paper item the prisoner receives on sentencing
//! (Prefabs/Items/Roleplay/SentenceSlip.et). It names the prisoner and the offence and shows the
//! online minutes left. The prisoner reads it by hovering it in the inventory (LF_JailSlipUIInfo).
//! Server-authoritative: only the server prints, refreshes, and stamps a slip. The fields
//! replicate as RplProps so the holder's client can read them; the minutes are bumped only when
//! the whole-minute value changes.
//!
//! PERSISTENCE. Nothing new is saved. The source of truth is the LF_Jail sentence (remaining
//! seconds, the "jail" payload) and the account (name, wanted level). A carried slip comes back
//! blank after a restart (the inventory snapshot keeps the prefab, not the print);
//! LF_JailSlip.Grant prints that blank slip again on the next spawn.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Police", description: "Printed sentence slip: prisoner, offence, online minutes left.")]
class LF_JailSlipComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_JailSlipComponent : ScriptComponent
{
	[RplProp()]
	protected string m_sHolderUid;

	[RplProp()]
	protected string m_sPrisonerName;

	[RplProp()]
	protected int m_iWantedLevel;

	[RplProp()]
	protected int m_iMinutesLeft;

	[RplProp()]
	protected bool m_bReleased;

	//------------------------------------------------------------------------------------------------
	//! Server: prints the sentence on a blank slip, or reprints the holder's own slip.
	//! \return False off the server, with no holder, or when the slip belongs to another prisoner.
	bool Print(string holderUid, string prisonerName, int wantedLevel, float remainingSeconds)
	{
		if (!Replication.IsServer())
			return false;

		if (holderUid.IsEmpty())
		{
			LF_Debug.Error("Jail", "slip not printed: no holder uid");
			return false;
		}

		if (!m_sHolderUid.IsEmpty() && m_sHolderUid != holderUid)
		{
			LF_Debug.Warn("Jail", "slip not printed: it belongs to another prisoner");
			return false;
		}

		m_sHolderUid = holderUid;
		m_sPrisonerName = prisonerName;
		m_iWantedLevel = wantedLevel;
		m_iMinutesLeft = LF_JailSlipRules.MinutesLeft(remainingSeconds);
		m_bReleased = false;
		Replication.BumpMe();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: shows the new online time left. Replicates only when the whole minute changes.
	//! \return True when the printed minutes changed.
	bool Refresh(float remainingSeconds)
	{
		if (!Replication.IsServer() || m_bReleased)
			return false;

		int minutes = LF_JailSlipRules.MinutesLeft(remainingSeconds);
		if (minutes == m_iMinutesLeft)
			return false;

		m_iMinutesLeft = minutes;
		Replication.BumpMe();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: stamps the slip as served. The prisoner keeps it.
	//! \return False when it is already stamped.
	bool Stamp()
	{
		if (!Replication.IsServer() || m_bReleased)
			return false;

		m_bReleased = true;
		m_iMinutesLeft = 0;
		Replication.BumpMe();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	string GetHolderUid()
	{
		return m_sHolderUid;
	}

	//------------------------------------------------------------------------------------------------
	string GetPrisonerName()
	{
		return m_sPrisonerName;
	}

	//------------------------------------------------------------------------------------------------
	int GetWantedLevel()
	{
		return m_iWantedLevel;
	}

	//------------------------------------------------------------------------------------------------
	int GetMinutesLeft()
	{
		return m_iMinutesLeft;
	}

	//------------------------------------------------------------------------------------------------
	bool IsReleased()
	{
		return m_bReleased;
	}

	//------------------------------------------------------------------------------------------------
	bool IsPrinted()
	{
		return !m_sHolderUid.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! The text printed on the paper, localized on the reading client.
	string Describe()
	{
		if (!IsPrinted())
			return WidgetManager.Translate("#LF-JailSlip_Blank");

		string text = WidgetManager.Translate("#LF-JailSlip_Prisoner", m_sPrisonerName);
		text = text + "\n" + WidgetManager.Translate("#LF-JailSlip_Offence", m_iWantedLevel);
		if (m_bReleased)
			return text + "\n" + WidgetManager.Translate("#LF-JailSlip_Stamped");

		return text + "\n" + WidgetManager.Translate("#LF-JailSlip_MinutesLeft", m_iMinutesLeft);
	}
}

//------------------------------------------------------------------------------------------------
//! Inventory read: hovering the slip shows what is printed on it.
[BaseContainerProps()]
class LF_JailSlipUIInfo : SCR_InventoryUIInfo
{
	//------------------------------------------------------------------------------------------------
	override string GetInventoryItemDescription(InventoryItemComponent item)
	{
		if (!item || !item.GetOwner())
			return super.GetInventoryItemDescription(item);

		LF_JailSlipComponent slip = LF_JailSlipComponent.Cast(item.GetOwner().FindComponent(LF_JailSlipComponent));
		if (!slip)
			return super.GetInventoryItemDescription(item);

		return slip.Describe();
	}
}

//------------------------------------------------------------------------------------------------
//! Pure rules, for LOGIC tests.
class LF_JailSlipRules
{
	//------------------------------------------------------------------------------------------------
	//! Whole online minutes left, rounded up: 61 seconds reads 2, 60 reads 1, 0 reads 0.
	static int MinutesLeft(float remainingSeconds)
	{
		if (remainingSeconds <= 0)
			return 0;

		int minutes = Math.Ceil(remainingSeconds / 60.0);
		return minutes;
	}
}

//------------------------------------------------------------------------------------------------
//! Server helpers: find the slip a prisoner carries and grant one.
class LF_JailSlip
{
	static const ResourceName PREFAB = "{4C1A9E7B52D83F60}Prefabs/Items/Roleplay/SentenceSlip.et";

	//------------------------------------------------------------------------------------------------
	//! Every sentence slip in the character's inventory or hands, each counted once.
	static void FindCarried(IEntity character, notnull array<LF_JailSlipComponent> outSlips)
	{
		outSlips.Clear();
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

			LF_JailSlipComponent slip = LF_JailSlipComponent.Cast(item.FindComponent(LF_JailSlipComponent));
			if (slip && outSlips.Find(slip) < 0)
				outSlips.Insert(slip);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The holder's own carried slip, else a blank carried slip, else null.
	static LF_JailSlipComponent FindFor(IEntity character, string holderUid)
	{
		array<LF_JailSlipComponent> slips = {};
		FindCarried(character, slips);
		LF_JailSlipComponent blank;
		foreach (LF_JailSlipComponent slip : slips)
		{
			if (slip.GetHolderUid() == holderUid)
				return slip;

			if (!blank && !slip.IsPrinted())
				blank = slip;
		}

		return blank;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the prisoner carries one slip printed with this sentence. A slip they already
	//! carry (their own, or a blank one) is reprinted instead of adding a second paper.
	//! \return The printed slip, or null when the character has no room or no inventory.
	static LF_JailSlipComponent Grant(IEntity character, string holderUid, string prisonerName, int wantedLevel, float remainingSeconds)
	{
		if (!Replication.IsServer())
		{
			LF_Debug.Error("Jail", "slip grant refused: not the server");
			return null;
		}

		if (!character || holderUid.IsEmpty())
		{
			LF_Debug.Error("Jail", "slip grant refused: no character or no holder uid");
			return null;
		}

		LF_JailSlipComponent carried = FindFor(character, holderUid);
		if (carried)
		{
			if (!carried.Print(holderUid, prisonerName, wantedLevel, remainingSeconds))
				return null;

			LF_Debug.Info("Jail", "slip reprinted for " + holderUid);
			return carried;
		}

		InventoryStorageManagerComponent storage = LF_Component<InventoryStorageManagerComponent>.Find(character);
		if (!storage)
		{
			LF_Debug.Error("Jail", "slip not granted: the prisoner has no inventory");
			return null;
		}

		IEntity item = LF_Utils.SpawnEntityPrefab(PREFAB, character.GetOrigin());
		if (!item)
		{
			LF_Debug.Error("Jail", "slip not granted: prefab did not spawn " + PREFAB);
			return null;
		}

		LF_JailSlipComponent slip = LF_JailSlipComponent.Cast(item.FindComponent(LF_JailSlipComponent));
		if (!slip)
		{
			LF_Debug.Error("Jail", "slip not granted: " + PREFAB + " has no LF_JailSlipComponent");
			SCR_EntityHelper.DeleteEntityAndChildren(item);
			return null;
		}

		slip.Print(holderUid, prisonerName, wantedLevel, remainingSeconds);
		if (!storage.TryInsertItem(item))
		{
			LF_Debug.Error("Jail", "slip not granted: no room in the inventory of " + holderUid);
			SCR_EntityHelper.DeleteEntityAndChildren(item);
			return null;
		}

		LF_Debug.Info("Jail", "slip handed to " + holderUid);
		return slip;
	}
}
