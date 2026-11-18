enum LF_EHandsetResult
{
	CONNECTED,
	NOT_SERVER,
	BAD_CALLER,
	NO_HANDSET,	//!< The radio is not in the caller's inventory (on the ground, someone else's).
	COOLDOWN,
	UNKNOWN_SERVICE,
	NO_ANSWER,
	NO_CASH
}

//------------------------------------------------------------------------------------------------
//! A carried radio set calls a service from wherever its carrier stands (#3155). The same call as
//! a payphone's: the route is checked first, nobody on duty takes no fee, then the fee is taken
//! from carried cash and LF_ServiceCalls.Place publishes LF_OnServiceCall with the caller's
//! position, so the board, pager and police dispatch subscribers see it like any payphone call.
//! Server only; the client asks with the action and sends nothing but which action it picked.
class LF_HandsetCall
{
	static const string FEATURE = "Handset";
	static const string TITLE = "#LF-Handset_Title";
	static const string SUMMARY = "#LF-Handset_Summary_Call";
	static const int DEFAULT_FEE = 5; // lf-fixed: the test and fallback fee; the shipped fee is the action prefab's m_iFee

	//! Seconds of the last connected call and of the last unanswered call, by player uid and service.
	protected static ref map<string, float> s_mLast;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mLast = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether handset is a radio in user's inventory or hands.
	static bool IsCarriedBy(IEntity handset, IEntity user)
	{
		if (!handset || !user)
			return false;

		array<BaseRadioComponent> radios = {};
		LF_ServiceRadio.FindCarried(user, radios);
		foreach (BaseRadioComponent radio : radios)
		{
			if (radio && radio.GetOwner() == handset)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from the action: the player calls service on the radio set handset.
	static LF_EHandsetResult ServerCall(IEntity user, IEntity handset, string service, int fee)
	{
		if (LF_Systems.Refuses("Comms", "handset call"))
			return LF_EHandsetResult.NOT_SERVER;

		if (!Replication.IsServer())
			return LF_EHandsetResult.NOT_SERVER;

		// A refused call costs nothing, so the shared comms limit (#834) bounds a spamming client.
		if (!LF_RateLimit.AllowCharacter(user, LF_ERequestKind.COMMS, "handset call"))
			return LF_EHandsetResult.COOLDOWN;

		float now = 0;
		if (GetGame() && GetGame().GetWorld())
			now = GetGame().GetWorld().GetWorldTime() / 1000;

		LF_EHandsetResult result = CallAt(now, LF_Utils.GetPlayerUid(user), user, handset, service, fee);
		LF_Utils.NotifyEntity(user, ResultKey(result), TITLE, 4.0);
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! The seam tests drive with their own clock (seconds).
	//! param players Who is on duty, for tests; null reads the connected players.
	static LF_EHandsetResult CallAt(float now, string uid, IEntity user, IEntity handset, string service, int fee, array<ref LF_ServiceCandidate> players = null)
	{
		if (!Replication.IsServer())
			return LF_EHandsetResult.NOT_SERVER;

		if (!user || uid.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "call refused: no player behind the radio");
			return LF_EHandsetResult.BAD_CALLER;
		}

		if (!IsCarriedBy(handset, user))
		{
			LF_Debug.Info(FEATURE, string.Format("%1 refused: calling %2 without carrying the radio", uid, service));
			return LF_EHandsetResult.NO_HANDSET;
		}

		if (!s_mLast)
			s_mLast = new map<string, float>();

		// One key per player and service: a miss or a call to the taxi never blocks a call to the police.
		string key = LF_Payphone.MissKey(uid, service);
		float last;
		if (!s_mLast.Find(key, last))
			last = -1;

		if (LF_Payphone.CooldownLeft(now, last, LF_ServiceCallsConfig.PlayerCooldown()) > 0)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 refused: called %2 again inside the cooldown", uid, service));
			return LF_EHandsetResult.COOLDOWN;
		}

		LF_ServiceRoute route = LF_ServiceCalls.Check(service, players);
		if (!route.m_bKnown)
			return LF_EHandsetResult.UNKNOWN_SERVICE;

		if (!route.IsAnswered())
		{
			s_mLast.Set(key, now);
			LF_Debug.Info(FEATURE, string.Format("%1 called %2: no answer, no fee taken", uid, service));
			return LF_EHandsetResult.NO_ANSWER;
		}

		// Checked before removing: RemoveCash takes what it finds, so a short purse would lose a part.
		fee = LF_Utils.MaxInt(fee, 0);
		if (fee > 0 && LF_MoneyUtils.GetCash(user) < fee)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 refused: %2 call needs %3 cash", uid, service, fee));
			return LF_EHandsetResult.NO_CASH;
		}

		if (fee > 0)
		{
			int taken = LF_MoneyUtils.RemoveCash(user, fee, "handset call");
			if (taken < fee)
			{
				LF_Debug.Error(FEATURE, string.Format("%1: fee %2 for %3 took only %4, call not placed", uid, fee, service, taken));
				return LF_EHandsetResult.NO_CASH;
			}
		}

		s_mLast.Set(key, now);
		vector position = user.GetOrigin();
		LF_ServiceCalls.Place(uid, service, LF_WantedReports.DescribePlace(position), SUMMARY, position, players);
		LF_Debug.Info(FEATURE, string.Format("%1 called %2 on a radio set at %3: fee %4 taken", uid, service, LF_Payphone.PhoneKey(position), fee));
		return LF_EHandsetResult.CONNECTED;
	}

	//------------------------------------------------------------------------------------------------
	//! Localization key the caller sees.
	static string ResultKey(LF_EHandsetResult result)
	{
		switch (result)
		{
			case LF_EHandsetResult.CONNECTED:
				return "#LF-Handset_Connected";
			case LF_EHandsetResult.COOLDOWN:
				return "#LF-Payphone_Cooldown";
			case LF_EHandsetResult.NO_ANSWER:
				return LF_ServiceCalls.NO_ANSWER_KEY;
			case LF_EHandsetResult.NO_CASH:
				return "#LF-Handset_NoCash";
			case LF_EHandsetResult.NO_HANDSET:
				return "#LF-Handset_NotCarried";
		}
		return "#LF-Payphone_Broken";
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_HandsetCall's statics (LF_StaticReset finds it by reflection).
class LF_HandsetCall_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_HandsetCall";
	}

	override void Apply()
	{
		LF_HandsetCall.ResetStatics();
	}
}
