//------------------------------------------------------------------------------------------------
//! Why a station refused a worker. OK is the only result that changed the station.
enum LF_EChainStation
{
	OK,
	NO_CHAIN,
	WRONG_STATE,
	TOO_FAR,
	LICENSE,
	LEVEL,
	EQUIPMENT,
	NO_ACCESS,
	MISSING_INPUTS,
	TOO_FAST,
	FAILED
}

//------------------------------------------------------------------------------------------------
//! One saved batch. Only stations holding a batch are saved, so a row always means loaded.
class LF_ChainStationRow : Managed
{
	string m_sStationKey;
	string m_sChainId;
	string m_sStepId;
	int m_iWork;
	int m_iNeeded;

	//------------------------------------------------------------------------------------------------
	LF_ChainStationRow Copy()
	{
		LF_ChainStationRow copy = new LF_ChainStationRow();
		copy.m_sStationKey = m_sStationKey;
		copy.m_sChainId = m_sChainId;
		copy.m_sStepId = m_sStepId;
		copy.m_iWork = m_iWork;
		copy.m_iNeeded = m_iNeeded;
		return copy;
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Chains", description: "A hands-on station of an illicit chain: a barrel, a still, a press. Actions: LF_ChainStationAction.")]
class LF_ChainStationComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! A hands-on station (#417). One component shape for a barrel, a still, a press or a cooker; the
//! station type and the step come from config (LF_ChainDef), not from the class. A worker loads a
//! batch, works it with held actions (each accepted action is one unit of work, whoever does it),
//! and collects the product. Progress belongs to the station, so several workers on one station,
//! or one worker on several stations, change how long a batch takes. The server decides
//! everything; the replicated percent only lets a client show the state in words.
//!
//! PERSISTENCE: a loaded station is saved by its m_sStationKey (the "chainstations" payload, save
//! version 20). The key must be unique in the world and stable; it is not the entity id.
class LF_ChainStationComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, "Stable key, unique in the world. The saved batch is matched by it")]
	protected string m_sStationKey;

	[Attribute("", UIWidgets.EditBox, "Site this station belongs to. Stations with the same site id share heat")]
	protected string m_sSiteId;

	[Attribute("", UIWidgets.EditBox, "Chain id in ResourceChains.conf")]
	protected string m_sChainId;

	[Attribute("", UIWidgets.EditBox, "Step id of that chain this station does")]
	protected string m_sStepId;

	[Attribute("1200", UIWidgets.Auto, "Fastest a worker's work repeats, in milliseconds. The hold is longer; a client that asks sooner is refused")]
	protected int m_iMinTickMs;

	[Attribute("3.5", UIWidgets.Auto, "Metres from the station a worker must stand")]
	protected float m_fReach;

	//! -1 empty, 0 to 99 being worked, 100 ready. Server writes, clients read.
	[RplProp()]
	protected int m_iPercent = -1;

	//! Smoke level 0 to 3 from the site heat (LF_ChainTrace). Server writes it only when the level
	//! changes; every machine shows the smoke from it, so a late joiner sees it too.
	[RplProp(onRplName: "OnSmokeChanged")]
	protected int m_iSmoke;

	protected static const ResourceName SMOKE_MEDIUM = "{059E612718472C0A}Particles/Enviroment/Smoke_Smokestack_Medium.ptc";
	protected static const ResourceName SMOKE_BIG = "{6EE3295AC363E562}Particles/Enviroment/Smoke_Smokestack_Big.ptc";
	protected static const vector SMOKE_OFFSET = "0 1 0";

	protected EntityID m_SmokeId = EntityID.INVALID;
	protected int m_iSmokeShown;

	//! Live stations by key. Lives here so the component's own OnDelete removes it.
	protected static ref map<string, LF_ChainStationComponent> s_mLive;

	protected ref LF_StationProgress m_Progress = new LF_StationProgress();
	protected ref map<string, float> m_mLastTick = new map<string, float>();

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
			return;

		LF_ChainStations.Register(this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		IEntity smoke = GetGame().GetWorld().FindEntityByID(m_SmokeId);
		if (smoke)
			SCR_EntityHelper.DeleteEntityAndChildren(smoke);

		if (s_mLive && s_mLive.Get(m_sStationKey) == this)
			s_mLive.Remove(m_sStationKey);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	static map<string, LF_ChainStationComponent> Live()
	{
		if (!s_mLive)
			s_mLive = new map<string, LF_ChainStationComponent>();

		return s_mLive;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetLive()
	{
		s_mLive = null;
	}

	//------------------------------------------------------------------------------------------------
	string GetStationKey()
	{
		return m_sStationKey;
	}

	//------------------------------------------------------------------------------------------------
	string GetSiteId()
	{
		return m_sSiteId;
	}

	//------------------------------------------------------------------------------------------------
	string GetChainId()
	{
		return m_sChainId;
	}

	//------------------------------------------------------------------------------------------------
	string GetStepId()
	{
		return m_sStepId;
	}

	//------------------------------------------------------------------------------------------------
	//! -1 empty, 0 to 99 being worked, 100 ready. Same answer on the server and on a client.
	int GetPercent()
	{
		if (Replication.IsServer())
			return m_Progress.Percent();

		return m_iPercent;
	}

	//------------------------------------------------------------------------------------------------
	LF_StationProgress GetProgress()
	{
		return m_Progress;
	}

	//------------------------------------------------------------------------------------------------
	//! Test and setup seam: a station built in script has no attributes to fill.
	void Configure(string stationKey, string siteId, string chainId, string stepId)
	{
		m_sStationKey = stationKey;
		m_sSiteId = siteId;
		m_sChainId = chainId;
		m_sStepId = stepId;
	}

	//------------------------------------------------------------------------------------------------
	//! The step this station runs, or false (logged once per call) when the config is missing.
	protected bool Resolve(out LF_ChainDef chain, out LF_ChainStepDef step)
	{
		chain = LF_ChainCatalog.FindChain(m_sChainId);
		if (chain)
			step = chain.FindStep(m_sStepId);

		if (!chain || !step)
		{
			LF_Debug.Error(LF_ChainSites.FEATURE, string.Format("station %1 refused: chain %2 step %3 is missing or disabled", m_sStationKey, m_sChainId, m_sStepId));
			return false;
		}

		if (step.m_sStationType.IsEmpty())
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a worker whose last accepted work was lastSeconds ago may work again.
	static bool TickAllowed(float lastSeconds, float nowSeconds, int minTickMs)
	{
		return (nowSeconds - lastSeconds) * 1000 >= minTickMs;
	}

	//------------------------------------------------------------------------------------------------
	protected void Sync()
	{
		int percent = m_Progress.Percent();
		if (percent == m_iPercent)
			return;

		m_iPercent = percent;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the smoke level this station shows (LF_ChainTrace). Replicates only on a change.
	void SetSmoke(int level)
	{
		if (level == m_iSmoke)
			return;

		m_iSmoke = level;
		Replication.BumpMe();
		ApplySmoke();
	}

	//------------------------------------------------------------------------------------------------
	int GetSmoke()
	{
		return m_iSmoke;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSmokeChanged()
	{
		ApplySmoke();
	}

	//------------------------------------------------------------------------------------------------
	//! Shows the smoke for m_iSmoke on this machine: a local particle on the station, replaced when
	//! the level changes. A dedicated server has no renderer and shows nothing.
	protected void ApplySmoke()
	{
		if (RplSession.Mode() == RplMode.Dedicated || m_iSmokeShown == m_iSmoke)
			return;

		m_iSmokeShown = m_iSmoke;
		IEntity old = GetGame().GetWorld().FindEntityByID(m_SmokeId);
		if (old)
			SCR_EntityHelper.DeleteEntityAndChildren(old);

		m_SmokeId = EntityID.INVALID;
		if (m_iSmoke < 1)
			return;

		ResourceName path = SMOKE_MEDIUM;
		if (m_iSmoke >= LF_ChainTrace.MAX_SMOKE)
			path = SMOKE_BIG;

		ParticleEffectEntitySpawnParams params = new ParticleEffectEntitySpawnParams();
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3] = GetOwner().GetOrigin() + SMOKE_OFFSET;
		params.PlayOnHeadlessClient = false;
		ParticleEffectEntity smoke = ParticleEffectEntity.SpawnParticleEffect(path, params);
		if (!smoke)
		{
			LF_Debug.Error(LF_ChainSites.FEATURE, string.Format("station %1: the smoke effect did not spawn", m_sStationKey));
			return;
		}

		float scale = 0.5;
		if (m_iSmoke >= 2)
			scale = 1.0;
		smoke.SetScale(scale);
		m_SmokeId = smoke.GetID();
	}

	//------------------------------------------------------------------------------------------------
	protected string WorkerKey(IEntity user)
	{
		string uid = LF_Utils.GetPlayerUID(user);
		if (!uid.IsEmpty())
			return uid;

		return user.GetID().ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Reach, hideout access and the licence, level and equipment gates.
	protected LF_EChainStation CheckWorker(IEntity user, LF_ChainDef chain, LF_ChainStepDef step)
	{
		if (!user)
			return LF_EChainStation.FAILED;

		if (vector.Distance(user.GetOrigin(), GetOwner().GetOrigin()) > m_fReach)
			return LF_EChainStation.TOO_FAR;

		LF_HideoutMachineComponent hideout = LF_Component<LF_HideoutMachineComponent>.Find(GetOwner());
		if (hideout && !LF_Hideout.CanProcessUser(user, hideout.GetHideoutId()))
			return LF_EChainStation.NO_ACCESS;

		LF_EChainGate gate = LF_ChainGate.ForPlayer(user, chain, step);
		if (gate == LF_EChainGate.LICENSE)
			return LF_EChainStation.LICENSE;
		if (gate == LF_EChainGate.LEVEL)
			return LF_EChainStation.LEVEL;
		if (gate == LF_EChainGate.EQUIPMENT)
			return LF_EChainStation.EQUIPMENT;

		return LF_EChainStation.OK;
	}

	//------------------------------------------------------------------------------------------------
	protected void Publish(string kind, string workerUid, LF_ChainDef chain, float nowSeconds)
	{
		float heat = LF_ChainSites.GetHeat(m_sSiteId, nowSeconds);
		LF_EventBus.Publish(LF_OnChainStep.Create(workerUid, m_sSiteId, chain.m_sChainId, m_sStepId, m_sStationKey, kind, heat, GetOwner().GetOrigin()));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a worker puts a batch in. Takes the step's inputs from their inventory, all or none.
	LF_EChainStation ServerLoad(IEntity user, float nowSeconds)
	{
		LF_ChainDef chain;
		LF_ChainStepDef step;
		if (!Resolve(chain, step))
			return LF_EChainStation.NO_CHAIN;

		if (m_Progress.IsLoaded())
			return LF_EChainStation.WRONG_STATE;

		LF_EChainStation access = CheckWorker(user, chain, step);
		if (access != LF_EChainStation.OK)
			return access;

		InventoryStorageManagerComponent inventory = LF_Component<InventoryStorageManagerComponent>.Find(user);
		if (!inventory || !LF_ProcessAction.TryConsumeInputs(inventory, step.m_aInputs))
			return LF_EChainStation.MISSING_INPUTS;

		m_Progress.Load(step.m_iWork);
		Sync();

		string uid = LF_Utils.GetPlayerUID(user);
		LF_Debug.Info(LF_ChainSites.FEATURE, string.Format("station %1 loaded by %2: %3/%4 needs %5 work", m_sStationKey, uid, m_sChainId, m_sStepId, step.m_iWork));
		Publish("load", uid, chain, nowSeconds);
		return LF_EChainStation.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one finished held action. Adds one unit of work to the station, whoever the worker is.
	LF_EChainStation ServerWork(IEntity user, float nowSeconds)
	{
		LF_ChainDef chain;
		LF_ChainStepDef step;
		if (!Resolve(chain, step))
			return LF_EChainStation.NO_CHAIN;

		if (!m_Progress.IsLoaded() || m_Progress.IsDone())
			return LF_EChainStation.WRONG_STATE;

		LF_EChainStation access = CheckWorker(user, chain, step);
		if (access != LF_EChainStation.OK)
			return access;

		string key = WorkerKey(user);
		float last = -100000;
		m_mLastTick.Find(key, last);
		if (!TickAllowed(last, nowSeconds, m_iMinTickMs))
		{
			LF_Debug.Warn(LF_ChainSites.FEATURE, string.Format("station %1: work from %2 refused, it came faster than a hold can finish", m_sStationKey, key));
			return LF_EChainStation.TOO_FAST;
		}

		m_mLastTick.Set(key, nowSeconds);
		bool finished = m_Progress.AddWork(1);
		Sync();

		string uid = LF_Utils.GetPlayerUID(user);
		float heat = LF_ChainSites.AddWork(m_sSiteId, 1, nowSeconds);
		float rangeM = LF_ChainSites.GetRange(m_sSiteId, nowSeconds);
		LF_Debug.Info(LF_ChainSites.FEATURE, string.Format("station %1 worked by %2: %3/%4, site heat %5", m_sStationKey, uid, m_Progress.GetWork(), m_Progress.GetNeeded(), heat));
		LF_EventBus.Publish(LF_OnChainWork.Create(uid, m_sSiteId, chain.m_sChainId, m_sStationKey, m_Progress.Percent(), heat, rangeM, GetOwner().GetOrigin()));

		if (finished)
		{
			LF_Debug.Info(LF_ChainSites.FEATURE, string.Format("station %1 ready", m_sStationKey));
			Publish("ready", uid, chain, nowSeconds);
		}

		return LF_EChainStation.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a worker takes a finished batch out. Anyone in reach may; the product is theirs.
	LF_EChainStation ServerCollect(IEntity user, float nowSeconds)
	{
		LF_ChainDef chain;
		LF_ChainStepDef step;
		if (!Resolve(chain, step))
			return LF_EChainStation.NO_CHAIN;

		if (!m_Progress.IsDone())
			return LF_EChainStation.WRONG_STATE;

		if (!user || vector.Distance(user.GetOrigin(), GetOwner().GetOrigin()) > m_fReach)
			return LF_EChainStation.TOO_FAR;

		InventoryStorageManagerComponent inventory = LF_Component<InventoryStorageManagerComponent>.Find(user);
		if (!inventory)
			return LF_EChainStation.FAILED;

		foreach (LF_ProcessingOutput output : step.m_aOutputs)
		{
			int added = LF_InventoryUtils.AddAmount(inventory, output.m_OutputPrefab, output.m_iOutputAmount, true);
			if (added != output.m_iOutputAmount)
			{
				LF_Debug.Error(LF_ChainSites.FEATURE, string.Format("station %1 collect failed: %2 of %3 x%4 delivered, the batch stays", m_sStationKey, added, output.m_OutputPrefab, output.m_iOutputAmount));
				return LF_EChainStation.FAILED;
			}
		}

		m_Progress.Clear();
		Sync();

		string uid = LF_Utils.GetPlayerUID(user);
		LF_Debug.Info(LF_ChainSites.FEATURE, string.Format("station %1 collected by %2", m_sStationKey, uid));
		Publish("collect", uid, chain, nowSeconds);
		return LF_EChainStation.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: the saved batch, or null for an empty station.
	LF_ChainStationRow Export()
	{
		if (!m_Progress.IsLoaded() || m_sStationKey.IsEmpty())
			return null;

		LF_ChainStationRow row = new LF_ChainStationRow();
		row.m_sStationKey = m_sStationKey;
		row.m_sChainId = m_sChainId;
		row.m_sStepId = m_sStepId;
		row.m_iWork = m_Progress.GetWork();
		row.m_iNeeded = m_Progress.GetNeeded();
		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: puts a saved batch back, or empties the station for a null row. A row for
	//! a different chain or step (the config changed) is dropped and logged. Idempotent.
	void Import(LF_ChainStationRow row)
	{
		if (!row)
		{
			m_Progress.Clear();
			Sync();
			return;
		}

		if (row.m_sChainId != m_sChainId || row.m_sStepId != m_sStepId)
		{
			LF_Debug.Warn(LF_ChainSites.FEATURE, string.Format("station %1: saved batch of %2/%3 dropped, the station now runs %4/%5", m_sStationKey, row.m_sChainId, row.m_sStepId, m_sChainId, m_sStepId));
			m_Progress.Clear();
			Sync();
			return;
		}

		m_Progress.Restore(true, row.m_iWork, row.m_iNeeded);
		Sync();
		LF_Debug.Info(LF_ChainSites.FEATURE, string.Format("station %1 restored at %2/%3", m_sStationKey, m_Progress.GetWork(), m_Progress.GetNeeded()));
	}

	//------------------------------------------------------------------------------------------------
	//! Localization key for a refusal. Empty for results the worker needs no text for.
	static string RefusalKey(LF_EChainStation result)
	{
		switch (result)
		{
			case LF_EChainStation.LICENSE: return "#LF-Chain_NeedLicense";
			case LF_EChainStation.LEVEL: return "#LF-Chain_NeedLevel";
			case LF_EChainStation.EQUIPMENT: return "#LF-Chain_NeedEquipment";
			case LF_EChainStation.NO_ACCESS: return "#EL-Hideout_NoUpgrade";
			case LF_EChainStation.MISSING_INPUTS: return "#LF-Chain_MissingInputs";
			case LF_EChainStation.TOO_FAR: return "#LF-Chain_TooFar";
			case LF_EChainStation.FAILED: return "#LF-Chain_Failed";
		}

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! The server's list of live stations, and the bridge to the save. A station registers itself
//! when it exists; a saved batch that arrives before its station waits here and is applied when
//! the station registers, and the other way round, so load order never matters.
class LF_ChainStations
{
	protected static ref map<string, ref LF_ChainStationRow> s_mPending;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_ChainStationRow> Pending()
	{
		if (!s_mPending)
			s_mPending = new map<string, ref LF_ChainStationRow>();

		return s_mPending;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		LF_ChainStationComponent.ResetLive();
		s_mPending = null;
	}

	//------------------------------------------------------------------------------------------------
	static void Register(LF_ChainStationComponent station)
	{
		if (!station)
			return;

		string key = station.GetStationKey();
		if (key.IsEmpty())
		{
			LF_Debug.Error(LF_ChainSites.FEATURE, "a chain station has no station key; it works but its batch is not saved");
			return;
		}

		LF_ChainStationComponent known = LF_ChainStationComponent.Live().Get(key);
		if (known && known != station)
		{
			LF_Debug.Error(LF_ChainSites.FEATURE, string.Format("station key %1 is used twice; the second station is not saved", key));
			return;
		}

		LF_ChainStationComponent.Live().Set(key, station);
		LF_ChainSites.Register(station.GetSiteId(), station.GetChainId(), station.GetOwner().GetOrigin());

		LF_ChainStationRow row = Pending().Get(key);
		if (row)
		{
			station.Import(row);
			Pending().Remove(key);
		}
	}

	//------------------------------------------------------------------------------------------------
	static LF_ChainStationComponent Find(string stationKey)
	{
		return LF_ChainStationComponent.Live().Get(stationKey);
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		return LF_ChainStationComponent.Live().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: every loaded station, plus saved batches whose station has not appeared.
	static array<ref LF_ChainStationRow> ExportAll()
	{
		array<ref LF_ChainStationRow> result = new array<ref LF_ChainStationRow>();
		foreach (string key, LF_ChainStationComponent station : LF_ChainStationComponent.Live())
		{
			LF_ChainStationRow row = station.Export();
			if (row)
				result.Insert(row);
		}

		foreach (string key, LF_ChainStationRow pending : Pending())
			result.Insert(pending.Copy());

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: the saved batches replace the live ones. A live station with no row is
	//! emptied; a row with no live station waits for it. Idempotent: a second load converges.
	static void ApplyAll(array<ref LF_ChainStationRow> records)
	{
		s_mPending = new map<string, ref LF_ChainStationRow>();
		if (records)
		{
			foreach (LF_ChainStationRow record : records)
			{
				if (!record || record.m_sStationKey.IsEmpty())
				{
					LF_Debug.Warn(LF_ChainSites.FEATURE, "load skipped a station row with no key");
					continue;
				}

				s_mPending.Set(record.m_sStationKey, record.Copy());
			}
		}

		foreach (string key, LF_ChainStationComponent station : LF_ChainStationComponent.Live())
		{
			LF_ChainStationRow row = s_mPending.Get(key);
			station.Import(row);
			if (row)
				s_mPending.Remove(key);
		}
	}
}
