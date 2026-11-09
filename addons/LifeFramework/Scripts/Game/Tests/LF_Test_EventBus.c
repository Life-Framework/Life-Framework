// red-proof: Observed 2026-11-09 with `tools/cli test --no-build --only LF_Test_EventBus`: LF_EventBus.Publish building its snapshot in reverse failed `[LFTEST] FAIL core/event-bus`, `- subscribers ran in subscription order and C ran after B failed (expected 'A:uid-2:#x;B:...', got 'C:uid-2:#x;B:...')`; restored, green.

// tier: LOGIC
//! The server event bus: publish and subscribe, subscription order, one failing or null
//! subscriber not stopping the rest, and unsubscribing during a publish.
class LF_Test_EventBus : LF_Test
{
	override string GetName()
	{
		return "core/event-bus";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_EventBus.ResetStatics();

		// Publish and subscribe: the right event class reaches the subscriber with its payload.
		LF_TestEventLog log = new LF_TestEventLog();
		LF_TestEventRecorder a = new LF_TestEventRecorder("A", log);
		LF_EventBus.Subscribe(LF_OnCrimeCommitted, a);
		ctx.Equal(1, LF_EventBus.Publish(LF_OnCrimeCommitted.Create("uid-1", "#offence")), "one subscriber handled the crime");
		ctx.EqualStr("A:uid-1:#offence;", log.m_sText, "the subscriber saw the payload");
		ctx.Equal(0, LF_EventBus.Publish(LF_OnArrested.Create("uid-1", 10, 1)), "an event of another class reaches nobody");
		ctx.EqualStr("A:uid-1:#offence;", log.m_sText, "the crime subscriber ignored the arrest");
		ctx.False(LF_EventBus.Subscribe(LF_OnCrimeCommitted, a), "the same subscriber is not added twice");
		ctx.False(LF_EventBus.Subscribe(LF_OnCrimeCommitted, null), "a null subscriber is refused");

		// Ordering and isolation: B fails, C still runs after it, in subscription order.
		log.m_sText = "";
		LF_TestEventRecorder b = new LF_TestEventRecorder("B", log);
		b.m_bFail = true;
		LF_TestEventRecorder c = new LF_TestEventRecorder("C", log);
		LF_EventBus.Subscribe(LF_OnCrimeCommitted, b);
		LF_EventBus.Subscribe(LF_OnCrimeCommitted, c);
		ctx.Equal(2, LF_EventBus.Publish(LF_OnCrimeCommitted.Create("uid-2", "#x")), "A and C handled it; B's error is not counted");
		ctx.EqualStr("A:uid-2:#x;B:uid-2:#x;C:uid-2:#x;", log.m_sText, "subscribers ran in subscription order and C ran after B failed");

		// Unsubscribing during a publish: A removes C while the publish runs. C still runs this
		// time (the list was copied), and is gone for the next publish.
		LF_TestEventRemover remover = new LF_TestEventRemover(log, c);
		LF_EventBus.ResetStatics();
		LF_EventBus.Subscribe(LF_OnCrimeCommitted, remover);
		LF_EventBus.Subscribe(LF_OnCrimeCommitted, c);
		log.m_sText = "";
		LF_EventBus.Publish(LF_OnCrimeCommitted.Create("uid-3", "#y"));
		ctx.EqualStr("R;C:uid-3:#y;", log.m_sText, "the subscriber removed mid-publish still ran once");
		ctx.Equal(1, LF_EventBus.SubscriberCount(LF_OnCrimeCommitted), "the removal took effect");
		log.m_sText = "";
		LF_EventBus.Publish(LF_OnCrimeCommitted.Create("uid-4", "#z"));
		ctx.EqualStr("R;", log.m_sText, "the next publish skips the removed subscriber");
		ctx.False(LF_EventBus.Unsubscribe(LF_OnCrimeCommitted, c), "removing it again reports false");

		LF_EventBus.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
class LF_TestEventLog : Managed
{
	string m_sText;
}

//------------------------------------------------------------------------------------------------
class LF_TestEventRecorder : LF_EventSubscriber
{
	protected string m_sName;
	protected ref LF_TestEventLog m_Log;
	bool m_bFail;

	void LF_TestEventRecorder(string name, LF_TestEventLog log)
	{
		m_sName = name;
		m_Log = log;
	}

	override string GetName()
	{
		return m_sName;
	}

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnCrimeCommitted crime = LF_OnCrimeCommitted.Cast(busEvent);
		if (crime)
			m_Log.m_sText = m_Log.m_sText + m_sName + ":" + crime.m_sPlayerUid + ":" + crime.m_sOffence + ";";

		return !m_bFail;
	}
}

//------------------------------------------------------------------------------------------------
class LF_TestEventRemover : LF_EventSubscriber
{
	protected ref LF_TestEventLog m_Log;
	protected LF_EventSubscriber m_Victim;

	void LF_TestEventRemover(LF_TestEventLog log, LF_EventSubscriber victim)
	{
		m_Log = log;
		m_Victim = victim;
	}

	override bool Handle(notnull LF_Event busEvent)
	{
		m_Log.m_sText = m_Log.m_sText + "R;";
		LF_EventBus.Unsubscribe(LF_OnCrimeCommitted, m_Victim);
		return true;
	}
}
