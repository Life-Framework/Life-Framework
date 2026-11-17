class LF_RobWeaponAction : ScriptedUserAction
{
	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Weapon prefab to steal", "et")]
	protected ResourceName m_WeaponPrefab;

	[Attribute(defvalue: "30", uiwidget: UIWidgets.Auto, desc: "Cooldown in seconds")]
	protected float m_fCooldown;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.Auto, desc: "Minimum police on duty required")]
	protected int m_iMinPoliceOnDuty;

	protected float m_fLastRobTime = LF_RobCooldown.NEVER;

	//------------------------------------------------------------------------------------------------
	//! Hidden while the Robbery system is off (#960).
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(LF_RobAction.SYSTEM);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		float nowMs = GetGame().GetWorld().GetWorldTime();
		string refusal = Gate(LF_Utils.GetPlayerUid(user), LF_AccountReplication.GetPoliceOnDutyCount(), m_iMinPoliceOnDuty, m_fCooldown, m_fLastRobTime, nowMs);
		if (refusal.IsEmpty())
			return true;

		if (refusal == LF_RobAction.REFUSED_POLICE)
			SetCannotPerformReason(LF_Terms.Text("#LF-Rob_NeedPoliceCount", m_iMinPoliceOnDuty.ToString()));
		else
			SetCannotPerformReason(ReasonKey(refusal));

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// A prompt shown just before Robbery was turned off (#960): no cooldown, and the right words.
		if (!LF_Systems.IsEnabled(LF_RobAction.SYSTEM))
		{
			if (Replication.IsServer() && LF_Systems.Refuses(LF_RobAction.SYSTEM, "weapon robbery"))
				LF_Utils.NotifyEntity(pUserEntity, "#LF-Systems_Off", "#EL-Robbery_Failed", 3.0);
			return;
		}

		// Recorded on both sides so the performing client's CanBePerformed also sees the cooldown.
		// The server judges the robbery against the time before this one, so it keeps that value.
		float nowMs = GetGame().GetWorld().GetWorldTime();
		float previousRobTime = m_fLastRobTime;
		m_fLastRobTime = nowMs;

		// The item grant, wanted level, and alert are authority-only.
		if (!Replication.IsServer())
			return;

		string playerUid = LF_Utils.GetPlayerUid(pUserEntity);
		int policeOnDuty = LF_AccountReplication.CountPoliceOnDuty();

		// A refused robbery tells the robber why (once) and does not start the cooldown.
		if (Refuse(playerUid, pUserEntity, policeOnDuty, m_iMinPoliceOnDuty, m_fCooldown, previousRobTime, nowMs))
		{
			m_fLastRobTime = previousRobTime;
			return;
		}

		if (CommitWeaponRobbery(playerUid, pUserEntity, m_WeaponPrefab, policeOnDuty, m_iMinPoliceOnDuty, m_fCooldown, previousRobTime, nowMs))
			LF_Utils.NotifyEntity(pUserEntity, "#EL-Weapon_Stolen", "#EL-Robbery_Successful", 3.0);
		else
			LF_Utils.NotifyEntity(pUserEntity, "#EL-Inventory_Full", "#EL-Robbery_Failed", 3.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Why this robbery may not happen now (an LF_RobAction.REFUSED_* reason), or empty when it may.
	//! Pure: the offer on a client and the server's commit both ask it. A grace period is judged by
	//! Refuse, because only the server knows a character's played time.
	//! \param lastRobTimeMs world time of the previous robbery of this action, or LF_RobCooldown.NEVER
	static string Gate(string playerUid, int policeOnDuty, int minPolice, float cooldownSeconds, float lastRobTimeMs, float nowMs)
	{
		if (LF_RobCooldown.IsActive(nowMs, lastRobTimeMs, cooldownSeconds))
			return LF_RobAction.REFUSED_COOLDOWN;

		// Only civilians can rob
		if (!LF_RobAction.IsFactionAllowedToRob(playerUid))
			return LF_RobAction.REFUSED_NOT_CIVILIAN;

		if (policeOnDuty < minPolice)
			return LF_RobAction.REFUSED_POLICE;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The offer text for a Gate refusal that is not the police count.
	protected static string ReasonKey(string refusal)
	{
		if (refusal == LF_RobAction.REFUSED_COOLDOWN)
			return "#LF-Rob_RecentlyRobbed";

		return "#EL-Rob_CiviliansOnly";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: true when the robbery is refused. Logs the reason and toasts the robber.
	//! Grace first (a new character may not rob, #1780), then Gate.
	static bool Refuse(string playerUid, IEntity user, int policeOnDuty, int minPolice, float cooldownSeconds, float lastRobTimeMs, float nowMs)
	{
		if (LF_Grace.Refuses(playerUid, "weapon robbery"))
		{
			LF_Grace.Notify(user);
			return true;
		}

		string refusal = Gate(playerUid, policeOnDuty, minPolice, cooldownSeconds, lastRobTimeMs, nowMs);
		if (refusal.IsEmpty())
			return false;

		LF_Debug.Warn("Crime", string.Format("weapon %1 (%2)", refusal, playerUid));
		if (refusal == LF_RobAction.REFUSED_POLICE)
			LF_Utils.NotifyEntity(user, "#LF-Rob_NeedPoliceCount", "#EL-Robbery_Failed", 3.0, minPolice.ToString());
		else
			LF_Utils.NotifyEntity(user, ReasonKey(refusal), "#EL-Robbery_Failed", 3.0);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server seam of PerformAction. Puts the weapon in the user's inventory. When that works and
	//! an account resolves for playerUid, raises wanted by the catalog's weapon theft amount and alerts police.
	//! Nothing is paid as cash and the bank is never touched.
	//! The robbery is refused (nothing delivered, no wanted) while the robber is in grace, is not a civilian,
	//! fewer than minPolice officers are on duty, or lastRobTimeMs is inside cooldownSeconds. The defaults
	//! (no police needed, no cooldown) are for callers that only exercise the delivery.
	//! \return True when the weapon was delivered.
	static bool CommitWeaponRobbery(string playerUid, IEntity user, ResourceName weaponPrefab, int policeOnDuty = 0, int minPolice = 0, float cooldownSeconds = 0, float lastRobTimeMs = -1, float nowMs = 0)
	{
		if (LF_Systems.Refuses(LF_RobAction.SYSTEM, "weapon robbery"))
			return false;

		if (LF_CuffEnforcement.Refuse(user, "weapon robbery"))
			return false;

		if (Refuse(playerUid, user, policeOnDuty, minPolice, cooldownSeconds, lastRobTimeMs, nowMs))
			return false;

		SCR_InventoryStorageManagerComponent inventoryManager = LF_Component<SCR_InventoryStorageManagerComponent>.Find(user);
		if (!inventoryManager)
			return false;

		array<IEntity> before = LF_InventoryUtils.FindItemsByPrefab(inventoryManager, weaponPrefab);
		if (!LF_InventoryUtils.AddItem(inventoryManager, weaponPrefab))
			return false;

		// The fence buys it back as stolen goods.
		LF_StolenGoods.MarkNew(user, weaponPrefab, before);

		LF_PlayerAccount account = LF_RobAction.FindAccount(playerUid);
		if (account)
		{
			LF_CrimeCatalog.ApplyWanted(account, playerUid, LF_WantedReports.OFFENCE_WEAPON_THEFT);
			LF_PlayerAccountManager.GetInstance().SaveAndReleaseAccount(account);
			LF_Dispatch.Report(playerUid, user, LF_WantedReports.OFFENCE_WEAPON_THEFT, user.GetOrigin());
		}

		return true;
	}
};
