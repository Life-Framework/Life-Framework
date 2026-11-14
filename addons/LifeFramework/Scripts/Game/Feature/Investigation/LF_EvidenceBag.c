//------------------------------------------------------------------------------------------------
//! An evidence bag (#268): what was found, where, when, and which crime report it belongs to. The
//! server stamps it when an officer bags a piece; the fields replicate so whoever holds the bag
//! can read it. The bagged piece rides inside the bag as a hidden child (the locker reads it, see
//! LF_EvidenceLocker). Bagging dresses the piece for travel: it is invisible and its physics is off.
[ComponentEditorProps(category: "LifeFramework/Feature/Investigation", description: "Evidence bag: place, time and crime report of the bagged piece.")]
class LF_EvidenceBagComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_EvidenceBagComponent : ScriptComponent
{
	[RplProp()]
	protected int m_iReport;

	[RplProp()]
	protected string m_sOffence;

	[RplProp()]
	protected string m_sPlace;

	[RplProp()]
	protected string m_sKind;

	[RplProp()]
	protected int m_iMinutes;

	//! What the piece says (LF_EvidenceRow.m_sNote); the locker hands it to the case board.
	[RplProp()]
	protected string m_sNote;

	//------------------------------------------------------------------------------------------------
	int GetReport()
	{
		return m_iReport;
	}

	//------------------------------------------------------------------------------------------------
	string GetOffence()
	{
		return m_sOffence;
	}

	//------------------------------------------------------------------------------------------------
	string GetPlace()
	{
		return m_sPlace;
	}

	//------------------------------------------------------------------------------------------------
	string GetKind()
	{
		return m_sKind;
	}

	//------------------------------------------------------------------------------------------------
	//! Minutes the piece lay at the scene before it was bagged.
	int GetMinutes()
	{
		return m_iMinutes;
	}

	//------------------------------------------------------------------------------------------------
	string GetNote()
	{
		return m_sNote;
	}

	//------------------------------------------------------------------------------------------------
	//! True once the server stamped a report on it.
	bool IsStamped()
	{
		return m_iReport > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: writes the record on the bag.
	void Stamp(int report, string offence, string place, string kind, int minutes, string note = "")
	{
		if (!Replication.IsServer())
			return;

		m_iReport = report;
		m_sOffence = offence;
		m_sPlace = place;
		m_sKind = kind;
		m_iMinutes = minutes;
		m_sNote = note;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: puts the piece inside the bag. It stops being a thing in the world.
	void Seal(notnull IEntity item)
	{
		if (!Replication.IsServer())
			return;

		Physics physics = item.GetPhysics();
		if (physics)
			physics.SetActive(ActiveState.INACTIVE);

		item.ClearFlags(EntityFlags.VISIBLE, true);
		GetOwner().AddChild(item, -1, EAddChildFlags.AUTO_TRANSFORM);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the bagged pieces (one per bag today).
	void GetContents(notnull array<IEntity> outItems)
	{
		IEntity child = GetOwner().GetChildren();
		while (child)
		{
			outItems.Insert(child);
			child = child.GetSibling();
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Interact action on scene evidence (a casing or a marker): an on-duty officer bags it. The offer
//! check reads the replicated account state; the server rechecks the officer before it bags.
class LF_BagEvidenceAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Investigation"))
			return false;

		return LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(user));
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(user)))
		{
			SetCannotPerformReason("#LF-Evidence_OnDutyOnly");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Investigation", "LF_BagEvidenceAction"))
			return;

		if (!Replication.IsServer())
			return;

		if (!LF_PoliceUtils.IsOnDutyOfficer(LF_PoliceUtils.GetAccountForEntity(pUserEntity)))
		{
			LF_Debug.Warn(LF_Evidence.FEATURE, string.Format("bag refused: %1 is not an on-duty officer", pUserEntity));
			return;
		}

		if (!LF_Evidence.Bag(pOwnerEntity, pUserEntity, System.GetTickCount()))
			return;

		LF_NotificationManagerComponent.NotifyPlayer(GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity), "#LF-Evidence_Title", "#LF-Evidence_Bagged", 5.0, LF_ENotificationType.INFO);
	}
}

//------------------------------------------------------------------------------------------------
//! Read the bag: place, time, and the crime report it belongs to.
class LF_ReadEvidenceBagAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Investigation"))
			return false;

		LF_EvidenceBagComponent bag = LF_EvidenceBagComponent.Cast(GetOwner().FindComponent(LF_EvidenceBagComponent));
		return bag && bag.IsStamped();
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Investigation", "LF_ReadEvidenceBagAction"))
			return;

		if (!pOwnerEntity || pUserEntity != LF_Utils.GetLocalCharacter())
			return;

		LF_EvidenceBagComponent bag = LF_EvidenceBagComponent.Cast(pOwnerEntity.FindComponent(LF_EvidenceBagComponent));
		LF_NotificationManagerComponent manager = LF_NotificationManagerComponent.GetInstance();
		if (!bag || !manager || !bag.IsStamped())
			return;

		string line = string.Format("#%1 %2, %3, %4 min", bag.GetReport(), WidgetManager.Translate(bag.GetOffence()), WidgetManager.Translate(bag.GetPlace()), bag.GetMinutes());
		manager.ShowNotificationToast("#LF-Evidence_Title", "#LF-Evidence_Read", 8.0, LF_ENotificationType.INFO, line);
	}
}

//------------------------------------------------------------------------------------------------
//! An item that leaves an inventory near an open scene may be dropped evidence (server).
modded class SCR_InventoryStorageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void OnItemRemoved(BaseInventoryStorageComponent storageOwner, IEntity item)
	{
		super.OnItemRemoved(storageOwner, item);
		if (Replication.IsServer() && item && GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(GetOwner()) > 0)
			LF_Evidence.WatchDrop(item);
	}
}
