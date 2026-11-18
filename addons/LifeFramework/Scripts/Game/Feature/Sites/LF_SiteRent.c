//------------------------------------------------------------------------------------------------
//! LF_SiteRent.c: rent a held contested site banks (#2683, gang war #1022). Each site sweep
//! (LF_ContestedSites.SweepSite) passes its holder and outcome here; a held site that is not
//! contested banks rentPerHour pro rata (LF_SitesConfig.For), capped at maxRentBanked. Rent is a
//! capped faucet like a wage: no pool account pays it. The bank stays on the site, so a capture
//! hands it to the new holder. Every full hour of banking logs the amount banked in that hour once
//! as an LF_OnMoneyMoved with source `site_rent` (no account side, so the ledger skips it).
//! Saved as the "siterent" payload.

//! One site's rent bank: the saved row and the live state.
class LF_SiteRentRow
{
	static const int VERSION = 1;

	int m_iVersion;
	string m_sHideoutId;
	//! Whole money units banked on the site.
	int m_iBanked;
	//! Unix seconds of the last sweep that banked rent, 0 when it never banked.
	int m_iLastRentAt;
	//! Banked since the last `site_rent` log line, not yet logged.
	int m_iUnlogged;
	//! Seconds of banking since the last `site_rent` log line.
	int m_iUnloggedSeconds;

	//------------------------------------------------------------------------------------------------
	static LF_SiteRentRow Create(string hideoutId, int banked, int lastRentAt)
	{
		LF_SiteRentRow row = new LF_SiteRentRow();
		row.m_iVersion = VERSION;
		row.m_sHideoutId = hideoutId;
		row.m_iBanked = banked;
		row.m_iLastRentAt = lastRentAt;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
class LF_SiteRent
{
	static const string FEATURE = "SiteRent";
	//! LF_OnMoneyMoved source of banked rent.
	static const string SOURCE = "site_rent";
	static const int LOG_SECONDS = 3600;	// lf-fixed: one money line per hour of banking

	protected static ref map<string, ref LF_SiteRentRow> s_mRows;
	//! Fractions of a unit not yet banked, per site (runtime only; under one unit is lost on a restart).
	protected static ref map<string, float> s_mCarry;
	//! Sub-second remainders of the hour clock, per site.
	protected static ref map<string, float> s_mSecondCarry;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mRows = null;
		s_mCarry = null;
		s_mSecondCarry = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_SiteRentRow> Rows()
	{
		if (!s_mRows)
			s_mRows = new map<string, ref LF_SiteRentRow>();

		return s_mRows;
	}

	//------------------------------------------------------------------------------------------------
	//! Banks one sweep's rent for a site (server). Nothing banks while the site is unheld, contested
	//! or changing hands this step, or once the bank reached maxRentBanked.
	//! \return The whole units banked by this call.
	static int Accrue(string hideoutId, string holderOrgId, LF_ESiteHoldOutcome outcome, float elapsedSeconds, int nowUnix)
	{
		if (hideoutId.IsEmpty() || holderOrgId.IsEmpty() || elapsedSeconds <= 0)
			return 0;

		if (outcome == LF_ESiteHoldOutcome.CONTESTED || outcome == LF_ESiteHoldOutcome.CAPTURED)
			return 0;

		LF_SiteSettings settings = LF_SitesConfig.For(hideoutId);
		if (settings.m_iRentPerHour <= 0)
			return 0;

		LF_SiteRentRow row = Rows().Get(hideoutId);
		if (!row)
		{
			row = LF_SiteRentRow.Create(hideoutId, 0, 0);
			Rows().Set(hideoutId, row);
		}

		if (!s_mCarry)
			s_mCarry = new map<string, float>();
		if (!s_mSecondCarry)
			s_mSecondCarry = new map<string, float>();

		int room = settings.m_iMaxRentBanked - row.m_iBanked;
		if (room <= 0)
		{
			s_mCarry.Set(hideoutId, 0);
			return 0;
		}

		float exact = s_mCarry.Get(hideoutId) + settings.m_iRentPerHour * elapsedSeconds / 3600.0;
		int whole = (int)exact;
		// Float sums of many sweeps land a hair under a whole unit (0.99999); keep that unit.
		if (exact - whole > 0.9999)
			whole++;
		float carry = exact - whole;
		if (carry < 0)
			carry = 0;

		if (whole >= room)
		{
			whole = room;
			carry = 0;
		}

		s_mCarry.Set(hideoutId, carry);
		float seconds = s_mSecondCarry.Get(hideoutId) + elapsedSeconds;
		int wholeSeconds = (int)seconds;
		s_mSecondCarry.Set(hideoutId, seconds - wholeSeconds);
		row.m_iUnloggedSeconds += wholeSeconds;
		if (whole > 0)
		{
			row.m_iBanked += whole;
			row.m_iUnlogged += whole;
			row.m_iLastRentAt = nowUnix;
		}

		if (row.m_iUnloggedSeconds >= LOG_SECONDS || row.m_iBanked >= settings.m_iMaxRentBanked)
			LogBanked(row, holderOrgId);

		return whole;
	}

	//------------------------------------------------------------------------------------------------
	//! One money line for what the site banked since the last one; the hour clock starts again.
	protected static void LogBanked(notnull LF_SiteRentRow row, string holderOrgId)
	{
		row.m_iUnloggedSeconds = 0;
		if (row.m_iUnlogged <= 0)
			return;

		int amount = row.m_iUnlogged;
		row.m_iUnlogged = 0;
		LF_OnMoneyMoved moved = LF_OnMoneyMoved.Create(holderOrgId, amount, SOURCE);
		moved.m_bCredit = true;
		moved.m_iBalanceAfter = row.m_iBanked;
		LF_Debug.Info(FEATURE, string.Format("%1 banked %2 rent for %3, bank %4", row.m_sHideoutId, amount, holderOrgId, row.m_iBanked));
		LF_EventBus.Publish(moved);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whole units banked on the site, 0 for a site that never banked.
	static int BankedOf(string hideoutId)
	{
		LF_SiteRentRow row = Rows().Get(hideoutId);
		if (!row)
			return 0;

		return row.m_iBanked;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Unix seconds of the site's last banking, 0 when it never banked.
	static int LastRentAtOf(string hideoutId)
	{
		LF_SiteRentRow row = Rows().Get(hideoutId);
		if (!row)
			return 0;

		return row.m_iLastRentAt;
	}

	//------------------------------------------------------------------------------------------------
	//! Rows for the "siterent" payload: every site with something banked or pending a log line.
	static array<ref LF_SiteRentRow> ExportAll()
	{
		array<ref LF_SiteRentRow> rows = {};
		foreach (string hideoutId, LF_SiteRentRow live : Rows())
		{
			if (!live || (live.m_iBanked <= 0 && live.m_iUnlogged <= 0))
				continue;

			LF_SiteRentRow row = LF_SiteRentRow.Create(hideoutId, live.m_iBanked, live.m_iLastRentAt);
			row.m_iUnlogged = live.m_iUnlogged;
			row.m_iUnloggedSeconds = live.m_iUnloggedSeconds;
			rows.Insert(row);
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the banks with the saved rows (load; idempotent: a second apply of the same rows
	//! gives the same banks). A row with no id, a newer version or a negative bank is dropped and
	//! logged; a bank above the site's cap is kept at the cap.
	//! \return Rows kept.
	static int ApplyAll(array<ref LF_SiteRentRow> rows)
	{
		Rows().Clear();
		s_mCarry = null;
		s_mSecondCarry = null;
		if (!rows)
			return 0;

		int kept;
		foreach (LF_SiteRentRow row : rows)
		{
			if (!row || row.m_sHideoutId.IsEmpty() || row.m_iVersion > LF_SiteRentRow.VERSION || row.m_iBanked < 0)
			{
				LF_Debug.Warn(FEATURE, "dropped a site rent row with no site, a newer version or a negative bank");
				continue;
			}

			int cap = LF_SitesConfig.For(row.m_sHideoutId).m_iMaxRentBanked;
			LF_SiteRentRow live = LF_SiteRentRow.Create(row.m_sHideoutId, LF_Utils.MinInt(row.m_iBanked, cap), row.m_iLastRentAt);
			live.m_iUnlogged = LF_Utils.ClampInt(row.m_iUnlogged, 0, live.m_iBanked);
			live.m_iUnloggedSeconds = LF_Utils.ClampInt(row.m_iUnloggedSeconds, 0, LOG_SECONDS);
			if (live.m_iBanked < row.m_iBanked)
				LF_Debug.Warn(FEATURE, string.Format("%1 saved bank %2 is above the cap %3, kept at the cap", row.m_sHideoutId, row.m_iBanked, cap));

			Rows().Set(live.m_sHideoutId, live);
			kept++;
		}

		return kept;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_SiteRent's statics (LF_StaticReset finds it by reflection).
class LF_SiteRent_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_SiteRent";
	}

	override void Apply()
	{
		LF_SiteRent.ResetStatics();
	}
}
