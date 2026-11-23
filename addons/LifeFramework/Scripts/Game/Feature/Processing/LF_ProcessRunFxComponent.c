[ComponentEditorProps(category: "LifeFramework/Processing", description: "A timed machine that shows it is running: fire glow, smoke and the time left on its action row")]
class LF_ProcessRunFxComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! A timed machine (#3229) that shows a run is under way (first-2h review B10): while LF_ProcessRun
//! holds a run at this machine, every rendering peer sees a fire glow and smoke, and the machine's
//! process row reads the time left ("Smelting: 3:12 left"). The server writes the run's end in seconds
//! of the replicated server clock (ChimeraWorld.GetServerTimestamp, the same on every peer), so a player
//! whose own clock is off, or a late joiner, counts down from the same end. Nothing is saved.
class LF_ProcessRunFxComponent : ScriptComponent
{
	static const string FEATURE = "Processing";

	[Attribute("{059E612718472C0A}Particles/Enviroment/Smoke_Smokestack_Medium.ptc", UIWidgets.ResourceNamePicker, "Smoke while a run is under way; empty for none", "ptc", category: "Running")]
	protected ResourceName m_Smoke;

	[Attribute("0 2.4 0", UIWidgets.EditBox, "Where the smoke rises from, in the machine's frame (metres)", category: "Running")]
	protected vector m_vSmokeOffset;

	[Attribute("0.5", UIWidgets.Auto, "Smoke scale", category: "Running")]
	protected float m_fSmokeScale;

	[Attribute("0 0.7 0.6", UIWidgets.EditBox, "Where the fire glow sits, in the machine's frame (metres)", category: "Running")]
	protected vector m_vGlowOffset;

	[Attribute("1 0.45 0.12", UIWidgets.EditBox, "Fire glow colour (red green blue, 0 to 1)", category: "Running")]
	protected vector m_vGlowColor;

	[Attribute("4", UIWidgets.Auto, "Fire glow radius (metres); 0 for no glow", category: "Running")]
	protected float m_fGlowRadius;

	//! Server-clock seconds (ServerNow) the run ends at; 0 when no run is under way. Server writes, every peer reads.
	[RplProp(onRplName: "OnRunChanged")]
	protected int m_iEnds;

	protected EntityID m_SmokeId = EntityID.INVALID;
	protected LightEntity m_Glow;
	protected bool m_bShown;

	//------------------------------------------------------------------------------------------------
	//! Pure: seconds left of a run ending at \p ends, at \p now (both server-clock seconds); 0 when it is over or none runs.
	static int SecondsLeft(int ends, int now)
	{
		if (ends <= 0)
			return 0;

		return LF_Utils.MaxInt(ends - now, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Whole seconds on the replicated server clock since the world started: the same number on the
	//! server and on every client, whatever their own wall clocks say (with System.GetUnixTime a client
	//! 45 s off read 4:41 left for a 240 s run). At least 1, so an end is never 0 (0 is "no run").
	static int ServerNow()
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return 1;

		WorldTimestamp start;
		float seconds = world.GetServerTimestamp().DiffSeconds(start);
		return LF_Utils.MaxInt(seconds, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: "m:ss" for a count of seconds (192 -> "3:12"); never negative.
	static string Clock(int seconds)
	{
		int left = LF_Utils.MaxInt(seconds, 0);
		int minutes = left / 60;
		int rest = left % 60;
		string pad = "";
		if (rest < 10)
			pad = "0";

		return minutes.ToString() + ":" + pad + rest.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds left of the run at this machine on this peer; 0 when none runs.
	int GetSecondsLeft()
	{
		return SecondsLeft(m_iEnds, ServerNow());
	}

	//------------------------------------------------------------------------------------------------
	bool IsRunning()
	{
		return GetSecondsLeft() > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a run of \p seconds started at \p machine. A machine without this component shows nothing.
	static void RunStarted(IEntity machine, float seconds)
	{
		LF_ProcessRunFxComponent fx = LF_Component<LF_ProcessRunFxComponent>.Find(machine);
		if (fx)
			fx.SetEnds(ServerNow() + Math.Ceil(seconds));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the run at \p machine is over (its outputs dropped).
	static void RunEnded(IEntity machine)
	{
		LF_ProcessRunFxComponent fx = LF_Component<LF_ProcessRunFxComponent>.Find(machine);
		if (fx)
			fx.SetEnds(0);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetEnds(int ends)
	{
		if (!Replication.IsServer() || m_iEnds == ends)
			return;

		m_iEnds = ends;
		Replication.BumpMe();
		LF_Debug.Info(FEATURE, string.Format("%1 shows a run: %2 s left", LF_Utils.GetPrefabName(GetOwner()), GetSecondsLeft()));
		// A listen host renders too, and an RplProp change never calls back on the server.
		ApplyFx();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRunChanged()
	{
		ApplyFx();
	}

	//------------------------------------------------------------------------------------------------
	//! Shows or clears the glow and smoke on this machine. A dedicated server has no renderer.
	protected void ApplyFx()
	{
		if (System.IsConsoleApp() || RplSession.Mode() == RplMode.Dedicated)
			return;

		bool show = m_iEnds > 0;
		if (show == m_bShown)
			return;

		m_bShown = show;
		ClearFx();
		if (!show)
			return;

		IEntity owner = GetOwner();
		if (!m_Smoke.IsEmpty())
		{
			ParticleEffectEntitySpawnParams params = new ParticleEffectEntitySpawnParams();
			Math3D.MatrixIdentity4(params.Transform);
			params.Transform[3] = owner.CoordToParent(m_vSmokeOffset);
			params.PlayOnHeadlessClient = false;
			ParticleEffectEntity smoke = ParticleEffectEntity.SpawnParticleEffect(m_Smoke, params);
			if (smoke)
			{
				smoke.SetScale(m_fSmokeScale);
				m_SmokeId = smoke.GetID();
			}
			else
			{
				LF_Debug.Error(FEATURE, "the running smoke did not spawn: " + m_Smoke);
			}
		}

		if (m_fGlowRadius > 0)
		{
			m_Glow = LightEntity.CreateLight(LightType.POINT, 0, m_fGlowRadius, Color.FromVector(m_vGlowColor), 6, m_vGlowOffset);
			if (m_Glow)
			{
				m_Glow.SetLensFlareType(LightLensFlareType.Disabled);
				owner.AddChild(m_Glow, -1);
			}
			else
			{
				LF_Debug.Error(FEATURE, "the running fire glow was not created");
			}
		}

		// Out by itself at the end even if the end never reaches this peer (a lost link, a late timer).
		GetGame().GetCallqueue().Remove(ExpireFx);
		GetGame().GetCallqueue().CallLater(ExpireFx, (GetSecondsLeft() + 5) * 1000, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void ExpireFx()
	{
		if (IsRunning())
			return;

		m_bShown = false;
		ClearFx();
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearFx()
	{
		BaseWorld world = GetGame().GetWorld();
		if (world && m_SmokeId != EntityID.INVALID)
		{
			IEntity smoke = world.FindEntityByID(m_SmokeId);
			if (smoke)
				SCR_EntityHelper.DeleteEntityAndChildren(smoke);
		}

		m_SmokeId = EntityID.INVALID;
		if (m_Glow)
			delete m_Glow;
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(ExpireFx);

		ClearFx();
		super.OnDelete(owner);
	}
}
