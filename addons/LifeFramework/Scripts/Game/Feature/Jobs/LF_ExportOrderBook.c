//------------------------------------------------------------------------------------------------
//! One open export order (#4410). Lives only in the server's book; orders are not saved.
class LF_ExportOrder
{
	int m_iId;
	string m_sTemplate;
	ResourceName m_Goods;
	string m_sGoodsName;
	string m_sDestination;
	int m_iCount;
	int m_iPay;
	int m_iDeadlineMinute;
	string m_sTakerUid;

	//------------------------------------------------------------------------------------------------
	bool IsTaken()
	{
		return !m_sTakerUid.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The deadline as the board shows it, "18:00".
	string DeadlineText()
	{
		int ofDay = m_iDeadlineMinute % LF_ExportOrderBook.DAY_MINUTES;
		int hour = ofDay / 60;
		int minute = ofDay % 60;
		return string.Format("%1:%2", hour.ToString(2), minute.ToString(2));
	}
}

//------------------------------------------------------------------------------------------------
//! The server-wide export order book (#4410): up to the configured number of orders open at once, one
//! more every refill interval while a slot is free. A hauler at the gate level takes one, hands the
//! goods in at the crane by the deadline and is paid the premium over wholesale; a taken order that
//! misses its deadline closes and costs the taker town standing, never money. Not saved: a restart
//! opens a fresh book. Time is in game minutes, counted on from the world clock (`CurrentMinute`).
class LF_ExportOrderBook
{
	static const string FEATURE = "ExportOrder";
	static const int DAY_MINUTES = 1440;

	static const string REFUSE_LEVEL = "#LF-ExportOrder_RefuseLevel";
	static const string REFUSE_GONE = "#LF-ExportOrder_RefuseGone";
	static const string REFUSE_TAKEN = "#LF-ExportOrder_RefuseTaken";
	static const string REFUSE_HOLDS_ONE = "#LF-ExportOrder_RefuseHoldsOne";
	static const string REFUSE_NONE_HELD = "#LF-ExportOrder_RefuseNoneHeld";
	static const string REFUSE_SHORT = "#LF-ExportOrder_RefuseShort";
	//! The short-count refusal that names how many units are still missing (%1); REFUSE_SHORT stays for a short with no count.
	static const string REFUSE_SHORT_COUNT = "#LF-ExportOrder_RefuseShortCount";

	protected static ref array<ref LF_ExportOrder> s_aOrders;
	protected static int s_iNextId = 1;
	protected static int s_iNextTemplate;
	protected static int s_iNextOpenMinute = -1;

	// The world clock gives a time of day; these count it on across midnight.
	protected static int s_iLastClockMinute = -1;
	protected static int s_iDayOffset;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aOrders = null;
		s_iNextId = 1;
		s_iNextTemplate = 0;
		s_iNextOpenMinute = -1;
		s_iLastClockMinute = -1;
		s_iDayOffset = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_ExportOrder> Orders()
	{
		if (!s_aOrders)
			s_aOrders = {};
		return s_aOrders;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The game minute now, counted on across midnight from the world clock; without a clock,
	//! the server's seconds stand in (a test world).
	static int CurrentMinute()
	{
		float hour = LF_CivilianJob.ClockHour();
		if (hour < 0)
			return Math.Floor(LF_JobGuards.Now() / 60);

		int ofDay = Math.Floor(hour * 60);
		// Only a big drop is midnight; a clock set back a little (an admin) is not a new day.
		if (s_iLastClockMinute >= 0 && s_iLastClockMinute - ofDay > DAY_MINUTES / 2)
			s_iDayOffset += DAY_MINUTES;

		s_iLastClockMinute = ofDay;
		return s_iDayOffset + ofDay;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: closes overdue orders (a taken one costs its taker standing) and opens new ones: all
	//! free slots on the first tick, then one per refill interval while a slot is free.
	static void Tick(int now)
	{
		LF_ExportOrderConfig config = LF_ExportOrderConfig.Get();
		if (!config)
			return;

		array<ref LF_ExportOrder> orders = Orders();
		for (int i = orders.Count() - 1; i >= 0; i--)
		{
			LF_ExportOrder order = orders[i];
			if (now < order.m_iDeadlineMinute)
				continue;

			if (order.IsTaken())
			{
				int moved = LF_Reputation.Add(order.m_sTakerUid, config.m_sStandingFaction, config.m_iLateStanding, "export order late");
				LF_Debug.Info(FEATURE, string.Format("order %1 missed its deadline: %2 loses standing (%3), the order closes", order.m_iId, order.m_sTakerUid, moved));
			}
			else
			{
				LF_Debug.Info(FEATURE, string.Format("order %1 expired untaken", order.m_iId));
			}

			orders.Remove(i);
		}

		if (config.m_aTemplates.IsEmpty())
			return;

		if (s_iNextOpenMinute < 0)
		{
			// Bounded: a kind with no price is skipped, so a book of unpriced kinds must not spin.
			int tries = config.m_iOpenSlots * config.m_aTemplates.Count();
			while (orders.Count() < config.m_iOpenSlots && tries > 0)
			{
				Open(config, now);
				tries--;
			}
			s_iNextOpenMinute = now + config.m_iRefillMinutes;
			return;
		}

		if (now < s_iNextOpenMinute)
			return;

		if (orders.Count() < config.m_iOpenSlots)
			Open(config, now);

		s_iNextOpenMinute = now + config.m_iRefillMinutes;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Open(notnull LF_ExportOrderConfig config, int now)
	{
		LF_ExportOrderTemplate template = config.m_aTemplates[s_iNextTemplate % config.m_aTemplates.Count()];
		s_iNextTemplate++;
		int pay = config.PayForTemplate(template);
		if (pay <= 0)
		{
			LF_Debug.Error(FEATURE, string.Format("export order kind %1 not opened: no wholesale price for %2 (template or market row)", template.m_sId, template.m_Goods));
			return;
		}

		LF_ExportOrder order = new LF_ExportOrder();
		order.m_iId = s_iNextId;
		s_iNextId++;
		order.m_sTemplate = template.m_sId;
		order.m_Goods = template.m_Goods;
		order.m_sGoodsName = template.m_sGoodsName;
		order.m_sDestination = template.m_sDestination;
		order.m_iCount = template.m_iCount;
		order.m_iPay = pay;
		order.m_iDeadlineMinute = now + template.m_iGameMinutes;
		Orders().Insert(order);
		LF_Debug.Info(FEATURE, string.Format("order %1 open: %2 x %3 for $%4 by %5", order.m_iId, order.m_iCount, order.m_sTemplate, order.m_iPay, order.DeadlineText()));
	}

	//------------------------------------------------------------------------------------------------
	//! \return The orders now in the book (open and taken), for the board. Do not keep the array.
	static array<ref LF_ExportOrder> List()
	{
		return Orders();
	}

	//------------------------------------------------------------------------------------------------
	static LF_ExportOrder Find(int id)
	{
		foreach (LF_ExportOrder order : Orders())
		{
			if (order.m_iId == id)
				return order;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The order `uid` has taken, or null.
	static LF_ExportOrder HeldBy(string uid)
	{
		if (uid.IsEmpty())
			return null;

		foreach (LF_ExportOrder order : Orders())
		{
			if (order.m_sTakerUid == uid)
				return order;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `uid`, whose job level is `level`, takes order `id`.
	//! \return Empty on success, else the refusal as a string key the player reads.
	static string Take(string uid, int level, int id)
	{
		LF_ExportOrderConfig config = LF_ExportOrderConfig.Get();
		if (!config || uid.IsEmpty())
			return REFUSE_GONE;

		if (level < config.m_iMinLevel)
		{
			LF_Debug.Info(FEATURE, string.Format("take refused: %1 is level %2, orders need %3", uid, level, config.m_iMinLevel));
			return REFUSE_LEVEL;
		}

		LF_ExportOrder order = Find(id);
		if (!order)
		{
			LF_Debug.Info(FEATURE, string.Format("take refused: order %1 is not open (%2)", id, uid));
			return REFUSE_GONE;
		}

		if (order.m_sTakerUid == uid)
			return string.Empty;

		if (order.IsTaken())
		{
			LF_Debug.Info(FEATURE, string.Format("take refused: order %1 is %2's (%3)", id, order.m_sTakerUid, uid));
			return REFUSE_TAKEN;
		}

		if (HeldBy(uid))
		{
			LF_Debug.Info(FEATURE, string.Format("take refused: %1 already holds an order", uid));
			return REFUSE_HOLDS_ONE;
		}

		order.m_sTakerUid = uid;
		LF_Debug.Info(FEATURE, string.Format("%1 took order %2", uid, id));
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `uid` hands in `delivered` units of its order's goods. The full count closes the order
	//! and returns its pay; a short count, or no order held, returns 0 and keeps the book as it was.
	//! The caller removes the goods before it pays (LF_ExportOrderCraneComponent.HandIn).
	static int HandIn(string uid, int delivered)
	{
		LF_ExportOrder order = HeldBy(uid);
		if (!order)
		{
			LF_Debug.Info(FEATURE, string.Format("hand-in refused: %1 holds no order", uid));
			return 0;
		}

		if (delivered < order.m_iCount)
		{
			LF_Debug.Info(FEATURE, string.Format("hand-in refused: %1 brought %2 of %3 for order %4; the order stays", uid, delivered, order.m_iCount, order.m_iId));
			return 0;
		}

		Orders().RemoveItem(order);
		LF_Debug.Info(FEATURE, string.Format("%1 handed in order %2: $%3", uid, order.m_iId, order.m_iPay));
		return order.m_iPay;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: forgets a player's hold, for a player who gives an order up. The order stays open.
	static void GiveUp(string uid)
	{
		LF_ExportOrder order = HeldBy(uid);
		if (order)
			order.m_sTakerUid = string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ExportOrderBook's statics (LF_StaticReset finds it by reflection).
class LF_ExportOrderBook_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ExportOrderBook";
	}

	override void Apply()
	{
		LF_ExportOrderBook.ResetStatics();
	}
}
