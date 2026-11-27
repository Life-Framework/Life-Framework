//------------------------------------------------------------------------------------------------
//! "Sell under the counter" on a barkeeper with LF_CounterOutletComponent (#4200). The offer is a client
//! hint; the server decides everything again in LF_CounterOutlet.ServerSell and says a refusal in words.
class LF_CounterOutletAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		SetCannotPerformReason("#LF-CounterOutlet_Nothing");
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Client hint only: a good the outlet's rows name. An on-duty officer still sees the row; the
	//! barkeeper says no in words.
	override bool CanBePerformedScript(IEntity user)
	{
		LF_CounterOutletComponent outlet = LF_Component<LF_CounterOutletComponent>.Find(GetOwner());
		if (!outlet)
			return false;

		array<ResourceName> goods = {};
		array<int> counts = {};
		return LF_CounterOutlet.Carried(user, outlet.m_sBuyerId, goods, counts) > 0;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity)
		{
			LF_Debug.Error(LF_CounterOutlet.FEATURE, "counter sale performed without a barkeeper or a user");
			return;
		}

		if (!Replication.IsServer())
			return;

		LF_CounterOutletComponent outlet = LF_Component<LF_CounterOutletComponent>.Find(pOwnerEntity);
		if (!outlet)
		{
			LF_Debug.Error(LF_CounterOutlet.FEATURE, "counter sale action sits on an entity without LF_CounterOutletComponent");
			return;
		}

		float now = GetGame().GetWorld().GetWorldTime() / 1000;
		bool police = LF_PlayerInteraction.IsOnDutyPolice(pUserEntity);
		int paid;
		int wait;
		LF_ECounterOutletSale result = LF_CounterOutlet.ServerSell(pUserEntity, pOwnerEntity, outlet.m_sBuyerId, outlet.m_fReach, outlet.m_iMinActionMs, police, System.GetUnixTime(), now, paid, wait);
		if (result == LF_ECounterOutletSale.OK)
		{
			LF_Utils.NotifyEntity(pUserEntity, "#LF-CounterOutlet_Sold", "#LF-CounterOutlet_Title", 4.0, LF_MoneyFormat.FormatMoney(paid));
			return;
		}

		LF_Debug.Info(LF_CounterOutlet.FEATURE, string.Format("counter sale refused (%1)", typename.EnumToString(LF_ECounterOutletSale, result)));
		string key = LF_CounterOutlet.RefusalKey(result);
		if (!key.IsEmpty())
			LF_Utils.NotifyEntity(pUserEntity, key, "#LF-CounterOutlet_Title", 4.0);
	}
}
