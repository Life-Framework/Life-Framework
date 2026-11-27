//------------------------------------------------------------------------------------------------
//! "Hand in the export order" at the dock crane spot (#4580). The row sits on the spot's pallet (the
//! prefab LF_ExportCraneSpot.et, which also carries LF_ExportOrderCraneComponent). It runs on the
//! server like any world action: the server checks the hauler stands at the spot, then hands the order
//! in through LF_ExportOrderCraneComponent.HandIn (goods out of a backed-up vehicle and the pockets,
//! then the pay) and toasts the pay or the refusal.
//------------------------------------------------------------------------------------------------
class LF_ExportCraneHandInAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled(LF_CivilianJob.SYSTEM))
			return false;

		IEntity owner = GetOwner();
		return owner && owner.FindComponent(LF_ExportOrderCraneComponent) != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-ExportCrane_HandIn";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity || !LF_NetworkUtils.ShouldApplyAction(pOwnerEntity))
			return;

		int paid;
		LF_ExportCraneHandIn.Serve(pUserEntity, pOwnerEntity, paid);
	}
}

//------------------------------------------------------------------------------------------------
class LF_ExportCraneHandIn
{
	//! Metres from the spot the hauler must stand in: the action context's reach with room for lag.
	static const float REACH_M = 6.0;	// lf-fixed: what "at the crane" means, not a server tunable

	//------------------------------------------------------------------------------------------------
	//! Server: `user` hands in the order it holds at `crane`. A hauler away from the spot is refused
	//! (logged, no toast: the action list never offers it there). The pay or the refusal is toasted.
	//! \param[out] paid The cash handed over, 0 on a refusal.
	//! \return Empty on success, else the refusal as a string key.
	static string Serve(IEntity user, IEntity crane, out int paid)
	{
		paid = 0;
		if (!user || !crane || (Replication.IsRunning() && !Replication.IsServer()))
			return LF_ExportOrderBook.REFUSE_NONE_HELD;

		if (!crane.FindComponent(LF_ExportOrderCraneComponent))
		{
			LF_Debug.Error(LF_ExportOrderBook.FEATURE, "export hand-in refused: the spot has no LF_ExportOrderCraneComponent");
			return LF_ExportOrderBook.REFUSE_NONE_HELD;
		}

		float distance = vector.Distance(user.GetOrigin(), crane.GetOrigin());
		if (distance > REACH_M)
		{
			LF_Debug.Warn(LF_ExportOrderBook.FEATURE, string.Format("export hand-in refused: %1 is %2 m from the crane spot", LF_Utils.GetPlayerUid(user), distance));
			return LF_ExportOrderBook.REFUSE_NONE_HELD;
		}

		int missing;
		string refusal = LF_ExportOrderCraneComponent.HandInCounted(user, crane, paid, missing);
		if (!refusal.IsEmpty())
		{
			LF_Utils.NotifyEntity(user, ToastKey(refusal, missing), "", 4.0, ToastArg(refusal, missing));
			return refusal;
		}

		LF_Debug.Info(LF_ExportOrderBook.FEATURE, string.Format("export order handed in by %1: %2", LF_Utils.GetPlayerUid(user), LF_MoneyFormat.FormatMoney(paid)));
		LF_Utils.NotifyEntity(user, "#LF-ExportCrane_Paid", "", 5.0, paid.ToString());	// the row writes {currency:sym}%1: LF_Terms.Text groups the amount (LF_MoneyFormat)
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The toast key of a refusal: a short count names the units still missing.
	static string ToastKey(string refusal, int missing)
	{
		if (refusal == LF_ExportOrderBook.REFUSE_SHORT && missing > 0)
			return LF_ExportOrderBook.REFUSE_SHORT_COUNT;

		return refusal;
	}

	//------------------------------------------------------------------------------------------------
	//! The %1 of that toast: the missing units for the count row, nothing for any other.
	static string ToastArg(string refusal, int missing)
	{
		if (refusal == LF_ExportOrderBook.REFUSE_SHORT && missing > 0)
			return missing.ToString();

		return string.Empty;
	}
}
