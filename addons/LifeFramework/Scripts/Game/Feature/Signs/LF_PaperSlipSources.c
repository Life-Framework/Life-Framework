// LF_PaperSlipSources.c: who hands out a slip (WO-W2.6, #4465). One bus subscriber, no edit to the
// features that pay: a paid job run prints a payslip, or a delivery note for a parcel run; a sale at
// a granary or a buyer prints a weigh slip. A trader's stack sale has no event of its own that
// names the item, so it hands out nothing here (follow-up on the trader).

//------------------------------------------------------------------------------------------------
class LF_PaperSlipSources : LF_EventSubscriber
{
	//! Money reasons (LF_OnMoneyMoved.m_sSource) of a sale at a buyer, with the item name key.
	static const string REASON_GRAIN = "grain-sale";
	static const string REASON_BUYER = "buyer-sale";
	static const string REASON_COUNTER = "counter-sale";

	protected static ref LF_PaperSlipSources s_Subscriber;

	//------------------------------------------------------------------------------------------------
	//! Server: subscribes once (game start). A bus reset drops the subscription; the next call restores it.
	static bool EnsureSubscribed()
	{
		if (!Replication.IsServer())
			return false;

		if (!s_Subscriber)
			s_Subscriber = new LF_PaperSlipSources();

		bool ok = SubscribeTo(LF_OnJobCompleted);
		ok = SubscribeTo(LF_OnMoneyMoved) && ok;
		return ok;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool SubscribeTo(typename eventType)
	{
		if (LF_EventBus.IsSubscribed(eventType, s_Subscriber))
			return true;

		return LF_EventBus.Subscribe(eventType, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_Subscriber)
		{
			LF_EventBus.Unsubscribe(LF_OnJobCompleted, s_Subscriber);
			LF_EventBus.Unsubscribe(LF_OnMoneyMoved, s_Subscriber);
		}

		s_Subscriber = null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The weigh slip item key for a money reason, or empty when the reason is no sale at a buyer.
	static string WeighItemFor(string reason)
	{
		if (reason == REASON_GRAIN)
			return "#LF-Paper_ItemGrain";

		if (reason == REASON_BUYER || reason == REASON_COUNTER)
			return "#LF-Paper_ItemGoods";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnJobCompleted job = LF_OnJobCompleted.Cast(busEvent);
		if (job)
		{
			if (job.m_iPay <= 0)
				return true;

			if (job.m_iJob == LF_EJobType.PARCEL)
				GiveTo(job.m_sPlayerUid, LF_PaperSlip.KIND_DELIVERY, "#LF-Paper_ItemParcel", 1, job.m_iPay);
			else
				GiveTo(job.m_sPlayerUid, LF_PaperSlip.KIND_PAYSLIP, "#LF-Paper_ItemShift", 1, job.m_iPay);

			return true;
		}

		LF_OnMoneyMoved money = LF_OnMoneyMoved.Cast(busEvent);
		if (money)
		{
			string item = WeighItemFor(money.m_sSource);
			if (item.IsEmpty() || !money.m_bCredit || money.m_iAmount <= 0)
				return true;

			GiveTo(money.m_sPlayerUid, LF_PaperSlip.KIND_WEIGH, item, 1, money.m_iAmount);
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static void GiveTo(string playerUid, string kindId, string item, int amount, int price)
	{
		LF_PlayerAccount account;
		IEntity body = LF_PoliceUtils.FindPlayerEntityByUid(playerUid, account);
		if (!body)
		{
			LF_Debug.Log(LF_PaperSlip.FEATURE, string.Format("%1 slip skipped: %2 has no body", kindId, playerUid));
			return;
		}

		LF_PaperSlip.Give(body, kindId, item, amount, price);
	}
}

//------------------------------------------------------------------------------------------------
//! Subscribes at game start without editing LF_GameModeRoleplay.c.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_PaperSlipSources.EnsureSubscribed();
	}
}

//------------------------------------------------------------------------------------------------
class LF_PaperSlipSources_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_PaperSlipSources";
	}

	override void Apply()
	{
		LF_PaperSlipSources.ResetStatics();
	}
}
