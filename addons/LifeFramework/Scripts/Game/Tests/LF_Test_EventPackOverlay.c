// red-proof: observed 2026-11-16 with `toolscli test --no-build --red LF_Test_EventPackOverlay`: with the hook on the overlay is not applied and the replace assertions fail (`replace: alpha has only the overlay's phase (expected 1, got 2)`, `replace: the cooldown is the overlay's (expected 10, got 300)`); `RED PROOF OK (all 1 listed test(s) fail with the hook on)`; without the hook green, `SUMMARY tier=only passed=1 failed=0`.
// tier: LOGIC
//! A pack's event config layers over the core by id (#1952, the shared override model #962): one
//! overlay adds an event, patches a phase timing of another (the fields it does not set keep the
//! core value), disables a third and replaces a fourth; an add of an id that exists and a patch of
//! one that does not are refused and change nothing. The core file itself loads and is valid.
class LF_Test_EventPackOverlay : LF_Test
{
	static const ResourceName OVERLAY = "{5A795328C2E8A902}Configs/Tests/LF_Test_EventOverlay.conf";

	override string GetName()
	{
		return "events/pack-overlay";
	}

	//------------------------------------------------------------------------------------------------
	//! The three core events the overlay works on: alpha (prep, hot), bravo (prep, hot with modules)
	//! and charlie (wrap).
	protected static LF_EventsConfig MakeCore()
	{
		LF_EventsConfig config = new LF_EventsConfig();
		config.m_aEvents = new array<ref LF_EventDef>();

		array<ref LF_EventPhase> alpha = {};
		alpha.Insert(LF_EventPhase.Make("prep", 5, 60));
		alpha.Insert(LF_EventPhase.Make("hot", 1, 0));
		config.m_aEvents.Insert(LF_EventDef.Make("alpha", alpha, 300));

		array<string> modules = {"mod_a", "mod_b"};
		array<ref LF_EventPhase> bravo = {};
		bravo.Insert(LF_EventPhase.Make("prep", 5, 60));
		bravo.Insert(LF_EventPhase.Make("hot", 1, 45, modules));
		config.m_aEvents.Insert(LF_EventDef.Make("bravo", bravo, 900));

		array<ref LF_EventPhase> charlie = {};
		charlie.Insert(LF_EventPhase.Make("wrap", 5, 0));
		config.m_aEvents.Insert(LF_EventDef.Make("charlie", charlie, 60));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EventDef FindIn(LF_EventsConfig config, string id)
	{
		foreach (LF_EventDef def : config.m_aEvents)
		{
			if (def && def.m_sEventId == id)
				return def;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		CheckCore(ctx);

		Resource resource = BaseContainerTools.LoadContainer(OVERLAY);
		ctx.True(resource && resource.IsValid() && resource.GetResource(), "the overlay fixture loads");
		if (!resource || !resource.IsValid() || !resource.GetResource())
			return;

		LF_EventsConfig config = MakeCore();
		int changed;
		if (!RedProof())
			changed = LF_EventOverlay.ApplyOverlay(config, resource.GetResource().ToBaseContainer(), OVERLAY);

		ctx.Equal(4, changed, "the overlay adds one, patches one, removes one and replaces one; two bad entries are refused");

		array<LF_EventDef> events = {};
		int skipped = LF_EventOverlay.Collect(config, events);
		ctx.Equal(0, skipped, "every event is still valid after the overlay");
		ctx.Equal(3, events.Count(), "three events: charlie left, delta came");

		LF_EventDef delta = FindIn(config, "delta");
		ctx.True(delta != null, "add: delta is in the list");
		if (delta)
		{
			ctx.Equal(1, delta.m_aPhases.Count(), "add: delta has its one phase");
			ctx.EqualStr("go", delta.m_aPhases[0].m_sName, "add: the phase name is the overlay's");
			ctx.EqualFloat(120, delta.m_fCooldownSeconds, 0.01, "add: the cooldown is the overlay's");
			ctx.EqualStr("", delta.m_sOp, "add: the stored record carries no verb");
		}

		LF_EventDef bravo = FindIn(config, "bravo");
		ctx.True(bravo != null, "patch: bravo is still in the list");
		if (bravo)
		{
			int hot = bravo.FindPhase("hot");
			ctx.True(hot >= 0, "patch: the hot phase is still there");
			if (hot >= 0)
			{
				ctx.EqualFloat(3, bravo.m_aPhases[hot].m_fCadenceSeconds, 0.01, "patch: the hot cadence is the overlay's");
				ctx.EqualFloat(45, bravo.m_aPhases[hot].m_fIdleTimeoutSeconds, 0.01, "patch: the idle timeout it did not set is kept");
				ctx.Equal(2, bravo.m_aPhases[hot].m_aModules.Count(), "patch: the modules it did not set are kept");
			}

			ctx.Equal(2, bravo.m_aPhases.Count(), "patch: no phase added or dropped");
			ctx.EqualFloat(5, bravo.m_aPhases[bravo.FindPhase("prep")].m_fCadenceSeconds, 0.01, "patch: a phase it did not name is untouched");
			ctx.EqualFloat(900, bravo.m_fCooldownSeconds, 0.01, "patch: the cooldown it did not set is kept");
			ctx.Equal(1, events.Find(bravo), "patch: bravo keeps its place in the list");
		}

		ctx.True(FindIn(config, "charlie") == null, "remove: charlie is disabled");

		LF_EventDef alpha = FindIn(config, "alpha");
		ctx.True(alpha != null, "replace: alpha is still in the list");
		if (alpha)
		{
			ctx.Equal(1, alpha.m_aPhases.Count(), "replace: alpha has only the overlay's phase");
			ctx.EqualStr("solo", alpha.m_aPhases[0].m_sName, "replace: the phase is the overlay's");
			ctx.EqualFloat(10, alpha.m_fCooldownSeconds, 0.01, "replace: the cooldown is the overlay's");
			ctx.Equal(0, events.Find(alpha), "replace: alpha keeps its place in the list");
		}

		ctx.True(FindIn(config, "ghost") == null, "a patch of an unknown id adds nothing");
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped core file loads through the same loader and its event is valid.
	protected void CheckCore(LF_TestContext ctx)
	{
		Resource resource = BaseContainerTools.LoadContainer(LF_EventOverlay.CONFIG_PATH);
		ctx.True(resource && resource.IsValid() && resource.GetResource(), "the core event file loads");
		if (!resource || !resource.IsValid() || !resource.GetResource())
			return;

		LF_EventsConfig core = LF_EventsConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
		ctx.True(core != null, "the core event file has the events root class");
		if (!core)
			return;

		array<LF_EventDef> events = {};
		ctx.Equal(0, LF_EventOverlay.Collect(core, events), "the core events are all usable");
		ctx.True(FindIn(core, "example") != null, "the neutral example event is in the core file");
	}
}
