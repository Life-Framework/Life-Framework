//------------------------------------------------------------------------------------------------
//! Server: the key a connected player's account is opened under (LF_Utils.GetAccountUID).
//!
//! On a listen host with the online service (the launcher's "Play on my own", hosting from the game
//! menu, a friend joining either) the platform identity arrives seconds after the player joined:
//! on playtest build 4 it was the NULL UUID when the registration form opened and the real one
//! about 15 s later. A stand-in key handed out in between opened the account under the player's
//! name, and the signed form then looked it up under the real identity, found nothing, and the
//! player was left without a character ("Your account is still loading").
//!
//! So while a real identity can still arrive, the answer is "" (wait) and every caller retries.
//! A listen host whose online service never answers falls back to the name stand-in after
//! LISTEN_WAIT_MS. If the real identity turns up after a stand-in was handed out, the stand-in
//! account moves onto the real key (LF_PlayerAccountManager.AdoptStandIn), so nothing opened under
//! the stand-in is lost or left behind as a second account.
class LF_PlayerIdentity
{
	//! How long a listen host waits for the platform identity before it keys the player by name.
	// lf-fixed: how long the online service may take, not a server choice; tests read it
	static const int LISTEN_WAIT_MS = 30000;

	//! Player id -> tick (ms) of the first lookup that found no real identity.
	protected static ref map<int, int> s_mWaitingSince = new map<int, int>();
	//! Player id -> the name stand-in handed out while a real identity could still arrive.
	protected static ref map<int, string> s_mStandIn = new map<int, string>();
	//! Player id -> session slot (agent-box sessions, LF_SESSION_IDENTITY): the lowest slot nobody
	//! connected holds. A client that quits and rejoins gets a new player id and the same slot back.
	protected static ref map<int, int> s_mSlot = new map<int, int>();

	//------------------------------------------------------------------------------------------------
	//! A platform identity that is not the player's own: empty, the NULL UUID, or a name-derived
	//! "00bbbddd-" stand-in (vanilla's peer-tool polyfill on a non-dedicated host, and ours).
	static bool IsStandIn(string identity)
	{
		if (identity.IsEmpty() || identity.StartsWith("00bbbddd-"))
			return true;

		UUID uuid = identity;
		return uuid.IsNull();
	}

	//------------------------------------------------------------------------------------------------
	//! The account key for this player. Pure apart from the per-player wait and stand-in state, so
	//! a test can drive a late identity (LF_Test_LateIdentity).
	//! \param identity What the platform answers for the player right now.
	//! \param dedicated RplSession is dedicated.
	//! \param sessionDefine LF_SESSION_IDENTITY (agent-box multi-client sessions).
	//! \param noBackend -noBackend, or Workbench: no real identity will ever arrive.
	//! \param nowMs The current tick in ms.
	//! \return the key, or "" while the caller has to wait for the identity.
	static string Resolve(int playerId, string identity, bool dedicated, bool sessionDefine, bool noBackend, int nowMs)
	{
		if (playerId <= 0)
			return "";

		if (!IsStandIn(identity))
		{
			OnRealIdentity(playerId, identity, nowMs);
			return identity;
		}

		if (!s_mWaitingSince.Contains(playerId))
			s_mWaitingSince.Set(playerId, nowMs);

		int waitedMs = nowMs - s_mWaitingSince.Get(playerId);
		LF_EIdentityFallback fallback = LF_Utils.ChooseIdentityFallback(dedicated, sessionDefine, noBackend, waitedMs);
		if (fallback == LF_EIdentityFallback.SESSION_SLOT)
			return LF_Utils.BuildSessionIdentity(SlotOf(playerId));

		if (fallback == LF_EIdentityFallback.WAIT)
			return "";

		// Vanilla's own polyfill when it gave one (lower-case; Workbench and listen-host saves are
		// keyed by it), else ours.
		string standIn = identity;
		if (!standIn.StartsWith("00bbbddd-"))
			standIn = LF_Utils.BuildLocalIdentity(playerId);

		// Only where a real identity could still arrive: the online service may answer late.
		if (!dedicated && !noBackend && !s_mStandIn.Contains(playerId))
		{
			s_mStandIn.Set(playerId, standIn);
			LF_Debug.Warn("Account", string.Format("player %1 has no platform identity after %2 s; their account is keyed by name (%3) until it arrives", playerId, waitedMs / 1000, standIn));
		}

		return standIn;
	}

	//------------------------------------------------------------------------------------------------
	protected static void OnRealIdentity(int playerId, string identity, int nowMs)
	{
		if (s_mWaitingSince.Contains(playerId))
		{
			LF_Debug.Info("Account", string.Format("platform identity of player %1 arrived after %2 ms", playerId, nowMs - s_mWaitingSince.Get(playerId)));
			s_mWaitingSince.Remove(playerId);
		}

		string standIn = s_mStandIn.Get(playerId);
		if (standIn.IsEmpty())
			return;

		s_mStandIn.Remove(playerId);
		LF_PlayerAccountManager.AdoptStandIn(standIn, identity);
	}

	//------------------------------------------------------------------------------------------------
	//! Milliseconds this player has waited for a real identity so far (0 when not waiting).
	static int WaitedMs(int playerId, int nowMs)
	{
		if (!s_mWaitingSince.Contains(playerId))
			return 0;

		return nowMs - s_mWaitingSince.Get(playerId);
	}

	//------------------------------------------------------------------------------------------------
	static bool HoldsSlot(int playerId)
	{
		return s_mSlot.Contains(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! The session slot of a connected player: the one it already holds, else the lowest slot (1..)
	//! no other connected player holds. First joiner 1, second 2; if client 2 quits and rejoins it is
	//! slot 2 again, so the harness identity (LF_Utils.BuildSessionIdentity(slot)) is the same account.
	static int SlotOf(int playerId)
	{
		int held;
		if (s_mSlot.Find(playerId, held))
			return held;

		int slot = 1;
		bool taken = true;
		while (taken)
		{
			taken = false;
			foreach (int other, int otherSlot : s_mSlot)
			{
				if (otherSlot == slot)
				{
					taken = true;
					break;
				}
			}
			if (taken)
				slot++;
		}

		s_mSlot.Set(playerId, slot);
		// The harness reads this line to learn which identity a client got (agent-box/session.mjs).
		LF_Debug.Info("Account", string.Format("session slot %1 for player %2: %3", slot, playerId, LF_Utils.BuildSessionIdentity(slot)));
		return slot;
	}

	//------------------------------------------------------------------------------------------------
	//! A player left: their wait, stand-in and session slot end with the connection.
	static void Forget(int playerId)
	{
		s_mWaitingSince.Remove(playerId);
		s_mStandIn.Remove(playerId);
		int slot;
		if (s_mSlot.Find(playerId, slot))
		{
			s_mSlot.Remove(playerId);
			LF_Debug.Info("Account", string.Format("session slot %1 released: player %2 left", slot, playerId));
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mWaitingSince.Clear();
		s_mStandIn.Clear();
		s_mSlot.Clear();
	}
}
