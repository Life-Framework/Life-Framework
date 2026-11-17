// LF_SiteHold.c: hold scoring for a contested site (#2678, gang war #1022).
// Pure maths, no world: LF_SiteHold.Step moves a site's state by the counted presence of each
// organisation and the elapsed seconds. The server's site system (later slices) filters who is
// present (alive, active members in range) and calls Step; nothing here reads the world.

//------------------------------------------------------------------------------------------------
//! What one Step did to a site.
enum LF_ESiteHoldOutcome
{
	//! Nothing moved: nobody qualifies, or a lone organisation is under the member minimum.
	NONE,
	//! The sole organisation present gained: the holder banked points, a challenger built progress.
	GAIN,
	//! Two or more organisations present: nothing moved.
	CONTESTED,
	//! Nobody present: a challenger's capture progress fell.
	DECAY,
	//! The site changed hands this step.
	CAPTURED,
	//! The holder alone on its site wiped a challenger's capture progress.
	DEFENDED
}

//------------------------------------------------------------------------------------------------
//! The tunables of one site's scoring, a plain value so the maths needs no config load.
class LF_SiteRules : Managed
{
	//! People of one organisation that must stand on the site before it gains or captures.
	int m_iMinMembersPresent;
	//! Most counted members one organisation contributes to capture speed.
	int m_iMaxCountedPerOrg;
	//! Capture progress, in counted-member seconds, at which the site flips.
	float m_fCaptureSeconds;
	//! Capture progress lost per hour while the site is empty.
	float m_fDecayPerHour;
	//! Seconds after a flip in which no new capture completes.
	float m_fFlipCooldownSeconds;
	//! Hold points the holder banks per hour of uncontested presence.
	float m_fPointsPerHour;
	//! Points a holder banks at most.
	int m_iMaxPoints;

	//------------------------------------------------------------------------------------------------
	//! Starting values until the site config (S2) owns them; the cap is the organisation config's.
	static LF_SiteRules Default()
	{
		LF_SiteRules rules = new LF_SiteRules();
		rules.m_iMinMembersPresent = 2;	// lf-fixed: default until the site config lands (#1022)
		rules.m_iMaxCountedPerOrg = 6;	// lf-fixed: default until the site config lands (#1022)
		rules.m_fCaptureSeconds = 600;	// lf-fixed: default until the site config lands (#1022)
		rules.m_fDecayPerHour = 1800;	// lf-fixed: default until the site config lands (#1022)
		rules.m_fFlipCooldownSeconds = 1800;	// lf-fixed: default until the site config lands (#1022)
		rules.m_fPointsPerHour = 60;	// lf-fixed: default until the site config lands (#1022)
		rules.m_iMaxPoints = LF_OrgConfig.HeldGroundMaxPoints();
		return rules;
	}
}

//------------------------------------------------------------------------------------------------
//! Who stands on the site: one row per organisation, already filtered to alive, active members.
class LF_SitePresence : Managed
{
	string m_sOrgId;
	//! People on the site.
	int m_iMembers;
	//! How many of them are inner-ring members (they count double).
	int m_iInnerRing;

	//------------------------------------------------------------------------------------------------
	static LF_SitePresence Create(string orgId, int members, int innerRing)
	{
		LF_SitePresence row = new LF_SitePresence();
		row.m_sOrgId = orgId;
		row.m_iMembers = members;
		row.m_iInnerRing = innerRing;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! One site's scoring state. Fractions are kept so short steps add up; the accessors floor.
class LF_SiteHoldState : Managed
{
	//! The organisation that holds the site; "" = nobody.
	string m_sHolderOrgId;
	//! The organisation capture progress belongs to; "" = none.
	string m_sCapturerOrgId;
	//! Counted-member seconds the capturer has built, 0 to LF_SiteRules.m_fCaptureSeconds.
	float m_fCaptureProgress;
	//! Banked points, 0 to LF_SiteRules.m_iMaxPoints.
	float m_fHoldPoints;
	//! Current uninterrupted hold, in seconds.
	float m_fHoldSeconds;
	//! Seconds this state has been stepped; the clock the flip cooldown runs on.
	float m_fClock;
	//! No capture completes before m_fClock reaches this.
	float m_fFlipCooldownUntil;

	//------------------------------------------------------------------------------------------------
	int HoldPoints()
	{
		return Math.Floor(m_fHoldPoints);
	}

	//------------------------------------------------------------------------------------------------
	int HoldSeconds()
	{
		return Math.Floor(m_fHoldSeconds);
	}
}

//------------------------------------------------------------------------------------------------
class LF_SiteHold
{
	//------------------------------------------------------------------------------------------------
	//! How much one organisation's presence counts: members plus the inner ring again, capped.
	static int CountedMembers(LF_SitePresence row, LF_SiteRules rules)
	{
		if (!row || row.m_iMembers <= 0)
			return 0;

		int inner = Math.Min(row.m_iInnerRing, row.m_iMembers);
		if (inner < 0)
			inner = 0;

		int counted = row.m_iMembers + inner;
		if (rules.m_iMaxCountedPerOrg > 0 && counted > rules.m_iMaxCountedPerOrg)
			counted = rules.m_iMaxCountedPerOrg;

		return counted;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the state by `elapsedSeconds` of the given presence and says what happened. Server only.
	//! A null state or rules, or a negative step, logs and changes nothing.
	static LF_ESiteHoldOutcome Step(LF_SiteHoldState state, array<ref LF_SitePresence> presence, float elapsedSeconds, LF_SiteRules rules)
	{
		if (!state || !rules)
		{
			LF_Debug.Error("Sites", "site-hold step refused: no state or no rules");
			return LF_ESiteHoldOutcome.NONE;
		}

		if (elapsedSeconds < 0)
		{
			LF_Debug.Warn("Sites", string.Format("site-hold step refused: negative step %1 s", elapsedSeconds));
			return LF_ESiteHoldOutcome.NONE;
		}

		state.m_fClock = state.m_fClock + elapsedSeconds;

		// Merge rows of one organisation, then keep those with somebody on the site.
		array<string> ids = {};
		array<int> members = {};
		array<int> counted = {};
		if (presence)
		{
			foreach (LF_SitePresence row : presence)
			{
				if (!row || row.m_sOrgId.IsEmpty() || row.m_iMembers <= 0)
					continue;

				int at = ids.Find(row.m_sOrgId);
				if (at < 0)
				{
					ids.Insert(row.m_sOrgId);
					members.Insert(row.m_iMembers);
					counted.Insert(CountedMembers(row, rules));
				}
				else
				{
					members[at] = members[at] + row.m_iMembers;
					int sum = counted[at] + CountedMembers(row, rules);
					if (rules.m_iMaxCountedPerOrg > 0 && sum > rules.m_iMaxCountedPerOrg)
						sum = rules.m_iMaxCountedPerOrg;

					counted[at] = sum;
				}
			}
		}

		if (ids.Count() >= 2)
			return LF_ESiteHoldOutcome.CONTESTED;

		if (ids.IsEmpty())
			return Decay(state, elapsedSeconds, rules);

		// Exactly one organisation is present; it gains only when it is big enough.
		if (members[0] < rules.m_iMinMembersPresent)
			return LF_ESiteHoldOutcome.NONE;

		if (ids[0] == state.m_sHolderOrgId)
			return Hold(state, elapsedSeconds, rules);

		return Capture(state, ids[0], counted[0], elapsedSeconds, rules);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ESiteHoldOutcome Decay(LF_SiteHoldState state, float elapsedSeconds, LF_SiteRules rules)
	{
		if (state.m_fCaptureProgress <= 0)
			return LF_ESiteHoldOutcome.NONE;

		state.m_fCaptureProgress = state.m_fCaptureProgress - rules.m_fDecayPerHour * elapsedSeconds / 3600;
		if (state.m_fCaptureProgress <= 0)
		{
			state.m_fCaptureProgress = 0;
			state.m_sCapturerOrgId = "";
		}

		return LF_ESiteHoldOutcome.DECAY;
	}

	//------------------------------------------------------------------------------------------------
	//! The holder alone: bank points and seconds, and wipe any challenger's progress.
	protected static LF_ESiteHoldOutcome Hold(LF_SiteHoldState state, float elapsedSeconds, LF_SiteRules rules)
	{
		state.m_fHoldSeconds = state.m_fHoldSeconds + elapsedSeconds;
		state.m_fHoldPoints = state.m_fHoldPoints + rules.m_fPointsPerHour * elapsedSeconds / 3600;
		if (state.m_fHoldPoints > rules.m_iMaxPoints)
			state.m_fHoldPoints = rules.m_iMaxPoints;

		if (state.m_fCaptureProgress > 0)
		{
			state.m_fCaptureProgress = 0;
			state.m_sCapturerOrgId = "";
			return LF_ESiteHoldOutcome.DEFENDED;
		}

		return LF_ESiteHoldOutcome.GAIN;
	}

	//------------------------------------------------------------------------------------------------
	//! A challenger (or anyone, on an unheld site) alone: build progress, flip at the threshold
	//! unless the flip cooldown still runs, in which case progress waits at the threshold.
	protected static LF_ESiteHoldOutcome Capture(LF_SiteHoldState state, string orgId, int counted, float elapsedSeconds, LF_SiteRules rules)
	{
		if (state.m_sCapturerOrgId != orgId)
		{
			state.m_sCapturerOrgId = orgId;
			state.m_fCaptureProgress = 0;
		}

		state.m_fCaptureProgress = state.m_fCaptureProgress + counted * elapsedSeconds;
		if (state.m_fCaptureProgress < rules.m_fCaptureSeconds)
			return LF_ESiteHoldOutcome.GAIN;

		state.m_fCaptureProgress = rules.m_fCaptureSeconds;
		if (state.m_fClock < state.m_fFlipCooldownUntil)
			return LF_ESiteHoldOutcome.GAIN;

		LF_Debug.Info("Sites", string.Format("site-hold flipped from '%1' to '%2'", state.m_sHolderOrgId, orgId));
		state.m_sHolderOrgId = orgId;
		state.m_sCapturerOrgId = "";
		state.m_fCaptureProgress = 0;
		state.m_fHoldPoints = 0;
		state.m_fHoldSeconds = 0;
		state.m_fFlipCooldownUntil = state.m_fClock + rules.m_fFlipCooldownSeconds;
		return LF_ESiteHoldOutcome.CAPTURED;
	}
}
