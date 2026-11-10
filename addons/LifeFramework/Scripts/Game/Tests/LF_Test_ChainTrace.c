// red-proof: observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_ChainTrace`: with the test chain's recipe left empty the catalog disabled it, no heat built, and the run failed `five work units at 10 heat each: half heat, half the range (expected 20, got 0)` and `the officer and the witness inside the range notice, the far officer does not (expected 2, got 0)`, `SUMMARY tier=only passed=0 failed=1`; recipe added, green.

// tier: LOGIC
//! The smell and smoke trace (#418): inside the heat-scaled range is noticed and outside is not,
//! an officer and a witness both notice, a notice repeats only after the cooldown, a cold site is
//! never noticed, and the smoke level follows the heat. Each notice is an LF_OnChainSmell.
class LF_Test_ChainTraceCounter : LF_EventSubscriber
{
	int m_iOfficers;
	int m_iWitnesses;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnChainSmell smell = LF_OnChainSmell.Cast(busEvent);
		if (!smell)
			return false;

		if (smell.m_bOfficer)
			m_iOfficers++;
		else
			m_iWitnesses++;

		return true;
	}
}

class LF_Test_ChainTrace : LF_Test
{
	override string GetName()
	{
		return "chains/trace";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ChainDef Chain()
	{
		LF_ChainDef chain = new LF_ChainDef();
		chain.m_sChainId = "trace-chain";
		chain.m_iTier = 1;
		chain.m_aSteps = new array<ref LF_ChainStepDef>();
		LF_ChainStepDef step = new LF_ChainStepDef();
		step.m_sStepId = "mix";
		step.m_sStationType = "barrel";
		step.m_aInputs = new array<ref LF_ProcessingInput>();
		step.m_aOutputs = new array<ref LF_ProcessingOutput>();
		step.m_iWork = 5;
		LF_ProcessingInput input = new LF_ProcessingInput();
		input.m_InputPrefab = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
		input.m_iInputAmount = 1;
		step.m_aInputs.Insert(input);
		LF_ProcessingOutput output = new LF_ProcessingOutput();
		output.m_OutputPrefab = "{1DBD99E6347DE1AD}Prefabs/Items/Drinks/AppleJuice.et";
		output.m_iOutputAmount = 1;
		step.m_aOutputs.Insert(output);
		chain.m_aSteps.Insert(step);
		chain.m_Heat = new LF_ChainHeatDef();
		chain.m_Heat.m_fPerWork = 10;
		chain.m_Heat.m_fDecayPerSecond = 1;
		chain.m_Heat.m_fMax = 100;
		chain.m_Heat.m_fRangeM = 40;
		return chain;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ChainTraceListener Listener(string uid, float metresFromSite, bool officer)
	{
		return LF_ChainTraceListener.Create(uid, Vector(metresFromSite, 0, 0), officer, null);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ChainCatalog.ResetStatics();
		LF_ChainSites.ResetStatics();
		LF_ChainTrace.ResetStatics();

		LF_ResourceChainConfig config = new LF_ResourceChainConfig();
		config.m_aChains = new array<ref LF_ChainDef>();
		LF_ChainDef chain = Chain();
		config.m_aChains.Insert(chain);
		LF_ChainCatalog.Build(config, false);

		ctx.True(LF_ChainTrace.Noticed(10, 20), "inside the range is noticed");
		ctx.True(!LF_ChainTrace.Noticed(30, 20), "outside the range is not");
		ctx.True(!LF_ChainTrace.Noticed(0, 0), "a cold site is never noticed");

		ctx.Equal(0, LF_ChainTrace.SmokeLevel(chain.m_Heat, 0), "a cold site shows no smoke");
		ctx.Equal(1, LF_ChainTrace.SmokeLevel(chain.m_Heat, 10), "a little heat is a thin plume");
		ctx.Equal(2, LF_ChainTrace.SmokeLevel(chain.m_Heat, 50), "half heat is a medium plume");
		ctx.Equal(3, LF_ChainTrace.SmokeLevel(chain.m_Heat, 100), "full heat is the big plume");

		LF_ChainSites.Register("trace-site", "trace-chain", Vector(0, 0, 0));
		LF_ChainSites.AddWork("trace-site", 5, 100);
		ctx.Equal(20, LF_ChainSites.GetRange("trace-site", 100), "five work units at 10 heat each: half heat, half the range");

		LF_Test_ChainTraceCounter counter = new LF_Test_ChainTraceCounter();
		LF_EventBus.Subscribe(LF_OnChainSmell, counter);

		array<ref LF_ChainTraceListener> listeners = {};
		listeners.Insert(Listener("near-officer", 10, true));
		listeners.Insert(Listener("near-witness", 15, false));
		listeners.Insert(Listener("far-officer", 30, true));

		ctx.Equal(2, LF_ChainTrace.Sweep(100, listeners), "the officer and the witness inside the range notice, the far officer does not");
		ctx.Equal(1, counter.m_iOfficers, "the near officer is told on the bus");
		ctx.Equal(1, counter.m_iWitnesses, "the near witness is told on the bus");
		ctx.Equal(0, LF_ChainTrace.Sweep(110, listeners), "the same listeners are not cued again inside the cooldown");
		ctx.Equal(0, LF_ChainTrace.Sweep(100 + LF_ChainTrace.CUE_COOLDOWN_S - 1, listeners), "one second short of the cooldown is still quiet");
		ctx.Equal(0, LF_ChainTrace.Sweep(1000, listeners), "the heat has faded by then: nothing to smell");

		LF_ChainSites.AddWork("trace-site", 5, 2000);
		ctx.Equal(2, LF_ChainTrace.Sweep(2000, listeners), "work again and the cooldown is long over: noticed again");

		ctx.True(LF_ChainSmellLead.TakeLead("early-site", 5), "the first notice raises a lead even in the first world seconds");
		ctx.True(LF_ChainSmellLead.TakeLead("trace-site", 2000), "the first notice at another site raises its own lead");
		ctx.True(!LF_ChainSmellLead.TakeLead("trace-site", 2001), "a second notice at the site does not");
		ctx.True(LF_ChainSmellLead.TakeLead("trace-site", 2000 + LF_ChainTrace.LEAD_COOLDOWN_S), "a lead is due again after the lead cooldown");

		LF_EventBus.Unsubscribe(LF_OnChainSmell, counter);
		LF_ChainSites.Remove("trace-site");
		LF_ChainTrace.ResetStatics();
		LF_ChainCatalog.ResetStatics();
	}
}
