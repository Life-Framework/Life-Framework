//------------------------------------------------------------------------------------------------
//! What a build-site call did (LF_BuildSiteComponent.ServerDeposit / ServerWork).
enum LF_EBuildSite
{
	OK,
	REFUSED_NO_SITE,	//!< the site names no plot, or the plot's tier is not in the config
	REFUSED_NOT_ALLOWED,	//!< the caller is neither the owner nor a key holder of the plot (or nobody owns it)
	REFUSED_FAR,	//!< the caller is out of reach of the site
	REFUSED_COMPLETE,	//!< every stage is built
	REFUSED_NO_ITEMS,	//!< the caller carries none of what the stage still lacks
	REFUSED_ITEMS_FIRST,	//!< work asked while the stage still lacks items
	REFUSED_TOO_FAST	//!< work asked again before a hold could finish
}

[ComponentEditorProps(category: "LifeFramework/Property", description: "A build site on a plot sign: takes deposits and work, advances the stage, shows the stage's shell")]
class LF_BuildSiteComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! A build site (#2650, slice of #747). It sits on the plot sign beside LF_PlotComponent and builds
//! the house of that plot, stage by stage, from LF_PropertyConfig (#2647) under LF_BuildStageRules
//! (#2648).
//!
//! STATE. Progress is the plot's LF_PropertyRecord (#2646): m_iBuildStage is the number of finished
//! stages (0 = bare plot), m_iWorkDone and m_sDeposited belong to the stage being built. Nothing is
//! kept on this entity, so the "properties" payload carries it over a restart. The shell is not
//! saved: Sync spawns the finished stage's shell prefab from the record after a load.
//!
//! SERVER ONLY decides. The owner and any key holder of the plot (a character carrying a key item
//! bound to the plot key, as at a house door; #2815) deposit what they carry (consumed from the inventory, counted in
//! the record) and works the site one unit per hold; when items and work are met the stage advances
//! and the shell swaps. Clients read only the replicated mirror for the action labels.
//!
//! SHELL ON CLIENTS. A shell prefab with an RplComponent is spawned by the server alone and reaches
//! every client, late joiners too, through replication. A plain prop (DirtPile has no RplComponent)
//! would never leave the server, so each client spawns its own copy from the replicated stage
//! (SpawnsShellHere); the server keeps its copy for collision and for a listen host.
//------------------------------------------------------------------------------------------------
class LF_BuildSiteComponent : ScriptComponent
{
	static const string FEATURE = "Property";
	//! Metres from the caller's character to the site.
	static const float REACH = 6.0; // lf-fixed: interaction reach around the plot sign
	//! Seconds one character must wait between two work units; a hold takes longer.
	static const float WORK_INTERVAL = 1.5; // lf-fixed: anti-spam floor under the 2 s held action
	//! How often the server mirrors the record and checks the shell (a load can change the record).
	static const int SYNC_MS = 2000; // lf-fixed: cheap map read per site

	[Attribute("0 0 8", UIWidgets.Coords, "Where the house shell stands, in the sign's local space (metres)")]
	protected vector m_vShellOffset;

	[RplProp(onRplName: "OnRplStage")]
	protected int m_iRplStage;
	[RplProp()]
	protected int m_iRplWork;
	[RplProp()]
	protected int m_iRplWorkNeeded;
	[RplProp()]
	protected string m_sRplMissing;
	[RplProp()]
	protected bool m_bRplOwned;
	[RplProp()]
	protected bool m_bRplComplete;

	protected LF_PlotComponent m_Plot;
	protected EntityID m_ShellId = EntityID.INVALID;
	protected int m_iShellStage = -1;
	protected bool m_bShellSpawnedHere;
	protected ref map<string, float> m_mLastWork = new map<string, float>();

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_Plot = LF_Component<LF_PlotComponent>.Find(owner);
		if (!m_Plot)
		{
			LF_Debug.Error(FEATURE, string.Format("build site %1 has no LF_PlotComponent; it builds nothing", LF_Utils.GetPrefabName(owner)));
			return;
		}

		GetGame().GetCallqueue().CallLater(Sync, SYNC_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Sync);
		DeleteShell();
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The plot key this site builds on; empty when the sign names none.
	string GetSiteKey()
	{
		if (!m_Plot)
			return "";

		return m_Plot.GetPlotKey();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The tier of the plot's house; empty when the config does not know the plot.
	string GetTierId()
	{
		return LF_PropertyConfig.PlotTierId(GetSiteKey());
	}

	//------------------------------------------------------------------------------------------------
	//! Replicated mirror, for the action labels. Finished stages, 0 = bare plot.
	int GetStage()
	{
		return m_iRplStage;
	}

	int GetWorkDone()
	{
		return m_iRplWork;
	}

	int GetWorkNeeded()
	{
		return m_iRplWorkNeeded;
	}

	//! "prefab=count;..." the stage being built still lacks; empty when its items are met.
	string GetMissing()
	{
		return m_sRplMissing;
	}

	bool IsOwned()
	{
		return m_bRplOwned;
	}

	bool IsComplete()
	{
		return m_bRplComplete;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the shell standing for the finished stage, null when none.
	IEntity GetShell()
	{
		if (m_ShellId == EntityID.INVALID)
			return null;

		return GetGame().GetWorld().FindEntityByID(m_ShellId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the stage whose shell is shown, -1 before the first sync.
	int GetShellStage()
	{
		return m_iShellStage;
	}

	//------------------------------------------------------------------------------------------------
	//! The stage being built (finished + 1), or null when all are finished or the config lacks it.
	protected LF_PropertyStageDef CurrentStage(LF_PropertyRecord record)
	{
		if (!record)
			return null;

		return LF_BuildStageRules.Stage(GetTierId(), record.m_iBuildStage + 1);
	}

	//------------------------------------------------------------------------------------------------
	//! param user The character to test.
	//! eturn True when the character carries (in the left-hand slot or anywhere in the inventory) a
	//! key item bound to this plot (#2815). Same key system as a house door (LF_LockComponent.UserHasValidKey);
	//! an unbound key matches nothing. Server and client may call it; it does not log.
	static bool UserHoldsKey(IEntity user, string plotKey)
	{
		if (!user || plotKey.IsEmpty())
			return false;

		CharacterControllerComponent characterController = LF_Component<CharacterControllerComponent>.Find(user);
		if (characterController && IsKeyFor(characterController.GetAttachedGadgetAtLeftHandSlot(), plotKey))
			return true;

		SCR_InventoryStorageManagerComponent inventoryManager = LF_Component<SCR_InventoryStorageManagerComponent>.Find(user);
		if (!inventoryManager)
			return false;

		array<IEntity> items = {};
		inventoryManager.GetItems(items);
		foreach (IEntity item : items)
		{
			if (IsKeyFor(item, plotKey))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsKeyFor(IEntity item, string plotKey)
	{
		if (!item)
			return false;

		LF_KeyComponent key = LF_Component<LF_KeyComponent>.Find(item);
		if (!key)
			return false;

		return LF_VehicleLockComponent.IdentifiersMatch(key.GetHouseIdentifier(), plotKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the key holders among the callers: the caller's own key when their character carries a
	//! key to this plot, none otherwise. Read again on every call, so a key handed away stops working
	//! at once (#2815).
	protected array<string> KeyHolders(IEntity user, string actorKey)
	{
		array<string> holders = {};
		if (!actorKey.IsEmpty() && UserHoldsKey(user, GetSiteKey()))
			holders.Insert(actorKey);

		return holders;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the checks every deposit and work share. Fills the record and the stage on OK.
	protected LF_EBuildSite Check(IEntity user, string actorKey, out LF_PropertyRecord record, out LF_PropertyStageDef def)
	{
		string key = GetSiteKey();
		if (key.IsEmpty() || GetTierId().IsEmpty())
			return LF_EBuildSite.REFUSED_NO_SITE;

		if (!user || vector.Distance(user.GetOrigin(), GetOwner().GetOrigin()) > REACH)
			return LF_EBuildSite.REFUSED_FAR;

		record = LF_Properties.Get(key);
		string ownerKey = LF_Properties.GetOwnerKey(key);
		if (!record || ownerKey.IsEmpty() || !LF_BuildStageRules.MayContribute(ownerKey, KeyHolders(user, actorKey), actorKey))
			return LF_EBuildSite.REFUSED_NOT_ALLOWED;

		def = CurrentStage(record);
		if (!def)
			return LF_EBuildSite.REFUSED_COMPLETE;

		return LF_EBuildSite.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the caller hands over everything they carry that the stage still lacks, up to what it
	//! lacks. Items leave the inventory and are counted in the record; an overflow stays carried.
	//! \param actorKey The caller's character key (LF_Utils.GetPlayerUID); never from the client.
	LF_EBuildSite ServerDeposit(IEntity user, string actorKey)
	{
		LF_PropertyRecord record;
		LF_PropertyStageDef def;
		LF_EBuildSite result = Check(user, actorKey, record, def);
		if (result != LF_EBuildSite.OK)
			return Refused("deposit", actorKey, result);

		string ownerKey = LF_Properties.GetOwnerKey(GetSiteKey());
		int total;
		if (def.m_aRequirements)
		{
			foreach (LF_PropertyStageReqDef req : def.m_aRequirements)
			{
				if (!req || req.m_Prefab.IsEmpty())
					continue;

				int missing = LF_BuildStageRules.Missing(def, record.m_sDeposited, req.m_Prefab);
				int carried = LF_InventoryUtils.GetAmount(user, req.m_Prefab);
				int wanted = LF_Utils.MinInt(missing, carried);
				if (wanted < 1)
					continue;

				int taken = LF_InventoryUtils.RemoveAmount(user, req.m_Prefab, wanted);
				if (taken < 1)
					continue;

				LF_BuildDepositResult deposit = LF_BuildStageRules.Deposit(def, record.m_sDeposited, req.m_Prefab, taken, ownerKey, KeyHolders(user, actorKey), actorKey);
				if (!deposit.m_bAccepted)
				{
					// Cannot happen after the checks above; give the items back rather than lose them.
					LF_InventoryUtils.AddAmount(user, req.m_Prefab, taken, true);
					LF_Debug.Error(FEATURE, string.Format("site %1 refused %2 x %3 after taking them (reason %4); returned", GetSiteKey(), taken, req.m_Prefab, deposit.m_iReason));
					continue;
				}

				record.m_sDeposited = deposit.m_sDeposited;
				total += taken;
				LF_Debug.Info(FEATURE, string.Format("site %1: %2 deposited %3 x %4", GetSiteKey(), actorKey, taken, req.m_Prefab));
			}
		}

		if (total < 1)
			return Refused("deposit", actorKey, LF_EBuildSite.REFUSED_NO_ITEMS);

		TryAdvance(record);
		return LF_EBuildSite.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one unit of hands-on work, once the stage's items are all in.
	//! \param now World time in seconds; two units from one character come at least WORK_INTERVAL apart.
	LF_EBuildSite ServerWork(IEntity user, string actorKey, float now)
	{
		LF_PropertyRecord record;
		LF_PropertyStageDef def;
		LF_EBuildSite result = Check(user, actorKey, record, def);
		if (result != LF_EBuildSite.OK)
			return Refused("work", actorKey, result);

		if (!LF_BuildStageRules.ItemsMet(def, record.m_sDeposited))
			return Refused("work", actorKey, LF_EBuildSite.REFUSED_ITEMS_FIRST);

		float last;
		if (m_mLastWork.Find(actorKey, last) && now - last < WORK_INTERVAL)
			return Refused("work", actorKey, LF_EBuildSite.REFUSED_TOO_FAST);

		m_mLastWork.Set(actorKey, now);
		record.m_iWorkDone = LF_BuildStageRules.WorkAfter(def, record.m_iWorkDone, 1);
		LF_Debug.Info(FEATURE, string.Format("site %1: %2 worked, %3/%4", GetSiteKey(), actorKey, record.m_iWorkDone, def.m_iWorkUnits));
		TryAdvance(record);
		return LF_EBuildSite.OK;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_EBuildSite Refused(string what, string actorKey, LF_EBuildSite reason)
	{
		LF_Debug.Info(FEATURE, string.Format("site %1 refused %2 by '%3': %4", GetSiteKey(), what, actorKey, typename.EnumToString(LF_EBuildSite, reason)));
		return reason;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a stage whose items and work are met is finished; the next starts empty.
	protected void TryAdvance(LF_PropertyRecord record)
	{
		LF_PropertyStageDef def = CurrentStage(record);
		if (def && LF_BuildStageRules.IsDone(def, record.m_sDeposited, record.m_iWorkDone))
		{
			LF_Properties.SetStage(record.m_sPropertyId, record.m_iBuildStage + 1);
			record.m_iWorkDone = 0;
			record.m_sDeposited = "";
			LF_Debug.Info(FEATURE, string.Format("site %1 finished stage %2 (%3)", record.m_sPropertyId, record.m_iBuildStage, def.m_sStageId));
		}

		Sync();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: mirrors the record for clients and shows the finished stage's shell. Runs after every
	//! change and every SYNC_MS, so a load or an admin stage change shows up without a hook.
	void Sync()
	{
		if (!Replication.IsServer())
		{
			RefreshShell(m_iRplStage);
			return;
		}

		LF_PropertyRecord record = LF_Properties.Get(GetSiteKey());
		int stage;
		int work;
		int needed;
		string missing;
		bool hasOwner;
		bool complete;
		if (record)
		{
			stage = record.m_iBuildStage;
			work = record.m_iWorkDone;
			hasOwner = !LF_Properties.GetOwnerKey(GetSiteKey()).IsEmpty();
			LF_PropertyStageDef def = CurrentStage(record);
			if (def)
			{
				needed = def.m_iWorkUnits;
				missing = LF_BuildStageRules.MissingText(def, record.m_sDeposited);
			}
			else
			{
				complete = stage > 0;
			}
		}

		if (stage != m_iRplStage || work != m_iRplWork || needed != m_iRplWorkNeeded || missing != m_sRplMissing || hasOwner != m_bRplOwned || complete != m_bRplComplete)
		{
			m_iRplStage = stage;
			m_iRplWork = work;
			m_iRplWorkNeeded = needed;
			m_sRplMissing = missing;
			m_bRplOwned = hasOwner;
			m_bRplComplete = complete;
			Replication.BumpMe();
		}

		RefreshShell(stage);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the replicated stage changed; show its shell now instead of at the next Sync.
	protected void OnRplStage()
	{
		RefreshShell(m_iRplStage);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when `prefab` loads and carries an RplComponent, so a server spawn replicates.
	static bool IsReplicatedPrefab(ResourceName prefab)
	{
		if (prefab.IsEmpty())
			return false;

		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return false;

		return SCR_BaseContainerTools.FindComponentSource(resource, RplComponent) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this machine spawns a shell itself. The server always does; a client only when the
	//! prefab is not replicated (the server's copy would never reach it).
	static bool SpawnsShellHere(bool isServer, bool replicated)
	{
		if (isServer)
			return true;

		return !replicated;
	}

	//------------------------------------------------------------------------------------------------
	//! Swaps the shell to the prefab of finished stage `stage` (none for a bare plot). The server
	//! passes the record's stage, a client the replicated one.
	protected void RefreshShell(int stage)
	{
		if (stage == m_iShellStage && (!m_bShellSpawnedHere || GetShell()))
			return;

		DeleteShell();
		m_iShellStage = stage;
		ResourceName prefab = LF_PropertyConfig.ShellPrefab(GetTierId(), stage);
		if (prefab.IsEmpty())
			return;

		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
		{
			LF_Debug.Error(FEATURE, string.Format("site %1: shell prefab of stage %2 does not load: %3", GetSiteKey(), stage, prefab));
			return;
		}

		if (!SpawnsShellHere(Replication.IsServer(), IsReplicatedPrefab(prefab)))
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		GetOwner().GetWorldTransform(params.Transform);
		params.Transform[3] = GetOwner().CoordToParent(m_vShellOffset);
		IEntity shell = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		if (!shell)
		{
			LF_Debug.Error(FEATURE, string.Format("site %1: shell of stage %2 did not spawn: %3", GetSiteKey(), stage, prefab));
			return;
		}

		m_ShellId = shell.GetID();
		m_bShellSpawnedHere = true;
		LF_Debug.Info(FEATURE, string.Format("site %1 shows stage %2 shell %3", GetSiteKey(), stage, prefab));
	}

	//------------------------------------------------------------------------------------------------
	protected void DeleteShell()
	{
		IEntity shell = GetShell();
		if (shell)
			SCR_EntityHelper.DeleteEntityAndChildren(shell);

		m_ShellId = EntityID.INVALID;
		m_bShellSpawnedHere = false;
	}
}
