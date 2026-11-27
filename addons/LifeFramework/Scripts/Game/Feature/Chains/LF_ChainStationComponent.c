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
	FAILED,
	SYSTEM_OFF	//!< The Chains system is off on this server (#1075).
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
	//! The system a server switches the stations off with (#1075). A saved batch stays in its station.
	static const string SYSTEM = "Chains";

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

	//! Mistake cue 0 to 2 (#2855, LF_ChainMistakes.CueLevel): off-colour smoke over the normal smoke,
	//! thicker one strike from ruin. Server writes it only on a change; a late joiner sees it too.
	[RplProp(onRplName: "OnCueChanged")]
	protected int m_iCue;

	protected static const ResourceName SMOKE_OFF_COLOUR = "{30B678ED32C177C2}Particles/Vehicle/Helicopter/Helicopter_Smoke_Damaged.ptc";
	protected static const ResourceName BLAST_EFFECT = "{3EC8ABC99657C8D1}Particles/Props/Explosion_Generic_Small.ptc";
	//! Work units of heat a blast adds: far past any chain's cap, so the site lands on its cap.
	protected static const int BLAST_HEAT_UNITS = 100000;

	protected EntityID m_CueId = EntityID.INVALID;
	protected int m_iCueShown;

	//! The batch's hidden strikes (session-only, see LF_ChainMistakes) and its seed, drawn at random
	//! for every new or restored batch so no player can work out which batch will blow.
	protected ref LF_ChainStrikes m_Strikes = new LF_ChainStrikes();
	protected int m_iBatchSeed;

	//! Server: a ruining mistake happened and the blast is queued (ServerBlow).
	protected bool m_bRuinPending;
	protected float m_fBlowAtSeconds;
	protected string m_sRuinUid;
	protected ref array<IEntity> m_aBlastHits;

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
		// owner.GetWorld(), not GetGame().GetWorld(): the World Editor deletes and
		// recreates entities (every move, undo, layer reload) in a world the game
		// instance does not own, where GetGame().GetWorld() is null and this threw.
		BaseWorld world = owner.GetWorld();
		if (world && m_SmokeId != EntityID.INVALID)
		{
			IEntity smoke = world.FindEntityByID(m_SmokeId);
			if (smoke)
				SCR_EntityHelper.DeleteEntityAndChildren(smoke);
		}

		if (world && m_CueId != EntityID.INVALID)
		{
			IEntity cue = world.FindEntityByID(m_CueId);
			if (cue)
				SCR_EntityHelper.DeleteEntityAndChildren(cue);
		}

		if (m_bRuinPending && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(ServerBlow);

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
	//! Server: the mistake cue this station shows. Replicates only on a change.
	protected void SetCue(int level)
	{
		if (level == m_iCue)
			return;

		m_iCue = level;
		Replication.BumpMe();
		ApplyCue();
	}

	//------------------------------------------------------------------------------------------------
	int GetCue()
	{
		return m_iCue;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCueChanged()
	{
		ApplyCue();
	}

	//------------------------------------------------------------------------------------------------
	//! Shows the off-colour smoke for m_iCue on this machine, beside the heat smoke. A dedicated
	//! server has no renderer and shows nothing.
	protected void ApplyCue()
	{
		if (RplSession.Mode() == RplMode.Dedicated || m_iCueShown == m_iCue)
			return;

		m_iCueShown = m_iCue;
		IEntity old = GetGame().GetWorld().FindEntityByID(m_CueId);
		if (old)
			SCR_EntityHelper.DeleteEntityAndChildren(old);

		m_CueId = EntityID.INVALID;
		if (m_iCue < 1)
			return;

		ParticleEffectEntitySpawnParams params = new ParticleEffectEntitySpawnParams();
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3] = GetOwner().GetOrigin() + SMOKE_OFFSET;
		params.PlayOnHeadlessClient = false;
		ParticleEffectEntity cue = ParticleEffectEntity.SpawnParticleEffect(SMOKE_OFF_COLOUR, params);
		if (!cue)
		{
			LF_Debug.Error(LF_ChainSites.FEATURE, string.Format("station %1: the off-colour smoke did not spawn", m_sStationKey));
			return;
		}

		float scale = 0.5;
		if (m_iCue >= 2)
			scale = 1.0;
		cue.SetScale(scale);
		m_CueId = cue.GetID();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: true while a ruined batch waits to blow. The station takes no work until then.
	bool IsRuinPending()
	{
		return m_bRuinPending;
	}

	//------------------------------------------------------------------------------------------------
	//! A new or emptied batch starts with no strikes, no cue and no queued blast.
	protected void ClearMistakes()
	{
		LF_ChainMistakes.Reset(m_Strikes);
		m_iBatchSeed = Math.RandomInt(0, 1000000000);
		if (m_bRuinPending)
			GetGame().GetCallqueue().Remove(ServerBlow);

		m_bRuinPending = false;
		m_sRuinUid = "";
		SetCue(0);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the ruining mistake. The station stops taking work and blows after the step's delay.
	protected void BeginRuin(LF_ChainStepDef step, string uid, float nowSeconds)
	{
		m_bRuinPending = true;
		m_sRuinUid = uid;
		int delayMs = LF_Utils.MaxInt(step.m_iRuinDelayMs, 0);
		m_fBlowAtSeconds = nowSeconds + delayMs / 1000.0;
		SetCue(2);
		LF_Debug.Warn(LF_ChainSites.FEATURE, string.Format("station %1: %2 ruined the batch of %3/%4; it blows in %5 ms", m_sStationKey, uid, m_sChainId, m_sStepId, delayMs));
		GetGame().GetCallqueue().CallLater(ServerBlow, delayMs, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the ruined batch blows (queued by BeginRuin; a test calls it at once). The batch is
	//! gone with no output, characters in reach are hurt through the damage manager, the site heat
	//! goes to its cap and the bus hears LF_OnChainBatchRuined. Does nothing unless a ruin is pending.
	void ServerBlow()
	{
		if (!m_bRuinPending)
			return;

		m_bRuinPending = false;
		GetGame().GetCallqueue().Remove(ServerBlow);

		m_Progress.Clear();
		Sync();
		LF_ChainMistakes.Reset(m_Strikes);
		SetCue(0);

		// An admin switched the system off while the blast was queued (#2882): the ruined batch is
		// gone, but nothing blows, no heat is added and nobody is hurt.
		if (!LF_Systems.IsEnabled(SYSTEM))
		{
			LF_Debug.Warn(LF_ChainSites.FEATURE, string.Format("station %1: the queued blast was cancelled, the Chains system is off (%2)", m_sStationKey, LF_Systems.WhyOff(SYSTEM)));
			m_sRuinUid = "";
			return;
		}

		LF_ChainDef chain;
		LF_ChainStepDef step;
		if (!Resolve(chain, step))
		{
			LF_Debug.Warn(LF_ChainSites.FEATURE, string.Format("station %1: the ruined batch was dropped without a blast, its step is gone from the config", m_sStationKey));
			m_sRuinUid = "";
			return;
		}

		vector origin = GetOwner().GetOrigin();
		float heat = LF_ChainSites.AddWork(m_sSiteId, BLAST_HEAT_UNITS, m_fBlowAtSeconds);
		int hurt = HurtNearby(origin, step.m_fBlastRadiusM, step.m_fBlastDamage);
		BroadcastBlast(origin);

		LF_Debug.Warn(LF_ChainSites.FEATURE, string.Format("station %1 blew: batch of %2/%3 lost, %4 hurt, site %5 heat %6", m_sStationKey, m_sChainId, m_sStepId, hurt, m_sSiteId, heat));
		LF_EventBus.Publish(LF_OnChainBatchRuined.Create(m_sRuinUid, m_sSiteId, chain.m_sChainId, m_sStepId, m_sStationKey, heat, origin));
		m_sRuinUid = "";
	}

	//------------------------------------------------------------------------------------------------
	//! Explosive damage on every character within radius, less with distance, through the base
	//! game's damage manager (zones, cuffs and downed rules still see the hit). \return How many.
	protected int HurtNearby(vector origin, float radius, float damage)
	{
		if (radius <= 0 || damage <= 0)
			return 0;

		// The sphere query misses a character moved this frame (its bounds update later), so the
		// base game's character list is read too; a character found twice is hurt once.
		m_aBlastHits = new array<IEntity>();
		GetGame().GetWorld().QueryEntitiesBySphere(origin, radius, CollectCharacter, null, EQueryEntitiesFlags.ALL);
		array<SCR_ChimeraCharacter> known = SCR_CharacterRegistrationComponent.GetChimeraCharacters();
		if (known)
		{
			foreach (SCR_ChimeraCharacter character : known)
			{
				if (character && vector.Distance(origin, character.GetOrigin()) <= radius && !m_aBlastHits.Contains(character))
					m_aBlastHits.Insert(character);
			}
		}

		int hurt = 0;
		int shielded = 0;
		foreach (IEntity target : m_aBlastHits)
		{
			if (!target)
				continue;

			// The blast has no instigator, so the zone hook (player-caused damage only) would let it
			// through: a safe zone still blocks it, and a wall or a rise of ground shields (#2882).
			if (LF_Zones.BlocksPlayerDamageAt(target.GetOrigin()) || !HasLineOfSight(origin, target))
			{
				shielded++;
				continue;
			}

			SCR_CharacterDamageManagerComponent manager = SCR_CharacterDamageManagerComponent.Cast(target.FindComponent(SCR_CharacterDamageManagerComponent));
			if (!manager)
				continue;

			HitZone zone = manager.GetDefaultHitZone();
			if (!zone)
				continue;

			float distance = vector.Distance(origin, target.GetOrigin());
			float amount = damage * (1 - Math.Min(distance / radius, 1) * 0.5); // lf-float: damage falloff
			vector hitPosDirNorm[3];
			hitPosDirNorm[0] = target.GetOrigin() + Vector(0, 1, 0);
			hitPosDirNorm[1] = vector.Direction(origin, target.GetOrigin()).Normalized();
			hitPosDirNorm[2] = vector.Up;
			SCR_DamageContext context = new SCR_DamageContext(EDamageType.EXPLOSIVE, amount, hitPosDirNorm, target, zone, Instigator.CreateInstigator(null), null, -1, -1);
			context.damageSource = GetOwner();
			manager.HandleDamage(context);
			hurt++;
		}

		m_aBlastHits = null;
		if (shielded > 0)
			LF_Debug.Info(LF_ChainSites.FEATURE, string.Format("station %1: the blast spared %2 in reach (safe zone or no line of sight)", m_sStationKey, shielded));

		return hurt;
	}

	//------------------------------------------------------------------------------------------------
	//! True when nothing solid lies between the still and the character's torso: the first thing a
	//! line from the still to the torso hits is the character itself (or something it carries).
	protected bool HasLineOfSight(vector origin, IEntity target)
	{
		TraceParam trace = new TraceParam();
		trace.Start = origin + "0 1 0";
		trace.End = target.GetOrigin() + "0 1 0";
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.Exclude = GetOwner();
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		float fraction = GetGame().GetWorld().TraceMove(trace, null);
		if (fraction >= 1)
			return true;

		IEntity hit = trace.TraceEnt;
		while (hit)
		{
			if (hit == target)
				return true;

			hit = hit.GetParent();
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectCharacter(IEntity e)
	{
		if (m_aBlastHits && SCR_ChimeraCharacter.Cast(e))
			m_aBlastHits.Insert(e);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the blast effect on every machine, spawned locally (only the call replicates).
	protected void BroadcastBlast(vector position)
	{
		Rpc(RpcDo_Blast, position);
		RpcDo_Blast(position);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Unreliable, RplRcver.Broadcast)]
	protected void RpcDo_Blast(vector position)
	{
		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		ParticleEffectEntitySpawnParams params = new ParticleEffectEntitySpawnParams();
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3] = position;
		params.PlayOnHeadlessClient = false;
		if (!ParticleEffectEntity.SpawnParticleEffect(BLAST_EFFECT, params))
			LF_Debug.Error(LF_ChainSites.FEATURE, string.Format("station %1: the blast effect did not spawn", m_sStationKey));
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
		if (LF_Systems.Refuses(SYSTEM, "station load at " + m_sStationKey))
			return LF_EChainStation.SYSTEM_OFF;

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
		ClearMistakes();

		string uid = LF_Utils.GetPlayerUID(user);
		LF_Debug.Info(LF_ChainSites.FEATURE, string.Format("station %1 loaded by %2: %3/%4 needs %5 work", m_sStationKey, uid, m_sChainId, m_sStepId, step.m_iWork));
		Publish("load", uid, chain, nowSeconds);
		return LF_EChainStation.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one finished held action. Adds one unit of work to the station, whoever the worker is.
	LF_EChainStation ServerWork(IEntity user, float nowSeconds)
	{
		if (LF_Systems.Refuses(SYSTEM, "station work at " + m_sStationKey))
			return LF_EChainStation.SYSTEM_OFF;

		LF_ChainDef chain;
		LF_ChainStepDef step;
		if (!Resolve(chain, step))
			return LF_EChainStation.NO_CHAIN;

		if (!m_Progress.IsLoaded() || m_Progress.IsDone() || m_bRuinPending)
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
		string uid = LF_Utils.GetPlayerUID(user);

		// A step with no mistake fields never strikes (#2855); a ruin wins over this tick's work.
		LF_EChainMistake mistake = LF_ChainMistakes.Tick(m_Strikes, step.m_iMistakeChancePct, step.m_iMaxMistakes, m_iBatchSeed);
		if (mistake == LF_EChainMistake.RUINED)
		{
			BeginRuin(step, uid, nowSeconds);
			return LF_EChainStation.OK;
		}

		if (mistake == LF_EChainMistake.STRIKE)
		{
			SetCue(LF_ChainMistakes.CueLevel(m_Strikes, step.m_iMaxMistakes));
			LF_Debug.Info(LF_ChainSites.FEATURE, string.Format("station %1: %2 made a mistake (%3 of %4)", m_sStationKey, uid, m_Strikes.m_iStrikes, step.m_iMaxMistakes));
		}

		bool finished = m_Progress.AddWork(1);
		Sync();

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
		if (LF_Systems.Refuses(SYSTEM, "station collect at " + m_sStationKey))
			return LF_EChainStation.SYSTEM_OFF;

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
		ClearMistakes();

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
		// Strikes are session-only: a restored or emptied station starts with none.
		ClearMistakes();
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
	//! #3150: where to get the licence this station's step needs, for its refusal toast; empty when none.
	string LicenceWhere()
	{
		LF_ChainDef chain;
		LF_ChainStepDef step;
		if (!Resolve(chain, step))
			return string.Empty;

		return LF_ChainGate.LicenceWhere(step);
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
			case LF_EChainStation.SYSTEM_OFF: return "#LF-Systems_Off";
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

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ChainStations's statics (LF_StaticReset finds it by reflection).
class LF_ChainStations_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ChainStations";
	}

	override void Apply()
	{
		LF_ChainStations.ResetStatics();
	}
}
