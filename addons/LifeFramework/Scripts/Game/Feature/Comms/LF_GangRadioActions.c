//------------------------------------------------------------------------------------------------
//! Why a gang radio tune was refused (or OK).
enum LF_EGangRadioTune
{
	OK,
	NOT_CARRIED,
	NOT_MEMBER,
	NO_BAND,
	NO_RADIO,
	TUNE_FAILED
}

//------------------------------------------------------------------------------------------------
//! Gang radio tuning (GA-C8, #4221). A member's carried gang radio is tuned on the server to the
//! gang's band (LF_GangRadio: a hidden frequency plus an encryption key); vanilla radio replication
//! carries the frequency and the key to the clients and vanilla voice does the rest, so members on
//! the band hear each other and nobody else does. The key never leaves the server through our code
//! and is never logged.
//!
//! The band is redrawn (a new frequency and a new key) whenever a member leaves or is kicked
//! (LF_OnOrganizationMemberLeft): the radios of the members still online move to the new band, the
//! leaver's radio is taken off it, and a member who was offline retunes from the radio's action
//! list. The band also lives only in memory, so after a restart every radio retunes the same way:
//! a stale frequency or key is simply overwritten.
class LF_GangRadioTune
{
	protected static ref LF_GangRadioMemberLeftListener s_Left;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_Left)
			LF_EventBus.Unsubscribe(LF_OnOrganizationMemberLeft, s_Left);

		s_Left = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: subscribes the rekey to member-left. Idempotent. Called before any band is drawn
	//! through this file, so every band a radio was tuned to is watched.
	static void Watch()
	{
		if (!s_Left)
			s_Left = new LF_GangRadioMemberLeftListener();

		LF_EventBus.Subscribe(LF_OnOrganizationMemberLeft, s_Left);
	}

	//------------------------------------------------------------------------------------------------
	//! True when the radio's first transceiver sits on the frequency with the key.
	static bool IsOn(BaseRadioComponent radio, int frequency, string key)
	{
		if (!radio || frequency <= 0 || key.IsEmpty())
			return false;

		BaseTransceiver transceiver = radio.GetTransceiver(0);
		if (!transceiver)
			return false;

		return transceiver.GetFrequency() == frequency && radio.GetEncryptionKey() == key;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the radio sits on the band.
	static bool IsOnBand(BaseRadioComponent radio, LF_GangRadioBand band)
	{
		if (!band)
			return false;

		return IsOn(radio, band.m_iFrequency, band.m_sKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: puts a radio on a band, powered, whatever it was tuned to before (a stale band from
	//! before a restart or a rekey is overwritten). \return False (logged) when it cannot tune there.
	static bool TuneTo(BaseRadioComponent radio, LF_GangRadioBand band)
	{
		if (!radio || !band)
		{
			LF_Debug.Error(LF_GangRadio.FEATURE, "tune refused: no radio or no band");
			return false;
		}

		if (!LF_ServiceRadio.Tune(radio, band.m_iFrequency))
			return false;

		radio.SetEncryptionKey(band.m_sKey);
		if (!radio.IsPowered())
			radio.SetPower(true);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: takes a radio off a gang band: no key, back on its lowest channel.
	static void Detune(BaseRadioComponent radio)
	{
		if (!radio)
			return;

		radio.SetEncryptionKey("");
		BaseTransceiver transceiver = radio.GetTransceiver(0);
		if (transceiver)
			transceiver.SetFrequency(transceiver.GetMinFrequency());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: moves every radio sitting on the old band to the new one (null: takes it off).
	//! Radios on any other frequency or key are left alone. \return How many radios moved.
	static int Retune(notnull array<BaseRadioComponent> radios, int oldFrequency, string oldKey, LF_GangRadioBand newBand)
	{
		int moved = 0;
		foreach (BaseRadioComponent radio : radios)
		{
			if (!IsOn(radio, oldFrequency, oldKey))
				continue;

			if (newBand)
			{
				if (TuneTo(radio, newBand))
					moved++;
			}
			else
			{
				Detune(radio);
				moved++;
			}
		}

		return moved;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a member pressed "Tune to gang channel" on a radio they carry.
	static LF_EGangRadioTune ServerTune(IEntity user, IEntity radioEntity)
	{
		if (!Replication.IsServer())
			return LF_EGangRadioTune.TUNE_FAILED;

		BaseRadioComponent radio;
		if (radioEntity)
			radio = BaseRadioComponent.Cast(radioEntity.FindComponent(BaseRadioComponent));

		if (!radio)
		{
			LF_Debug.Error(LF_GangRadio.FEATURE, "tune refused: the item has no radio");
			return LF_EGangRadioTune.NO_RADIO;
		}

		if (!LF_HandsetCall.IsCarriedBy(radioEntity, user))
		{
			LF_Debug.Info(LF_GangRadio.FEATURE, "tune refused: the radio is not carried by the user");
			return LF_EGangRadioTune.NOT_CARRIED;
		}

		string characterId = LF_OrganizationPlayer.CharacterId(user);
		LF_Organization org;
		if (!characterId.IsEmpty())
			org = LF_OrganizationManager.GetByCharacter(characterId);

		if (!org)
		{
			LF_Debug.Info(LF_GangRadio.FEATURE, string.Format("tune refused: %1 is in no gang", characterId));
			return LF_EGangRadioTune.NOT_MEMBER;
		}

		Watch();
		LF_GangRadioBand band = LF_GangRadio.BandFor(org.GetId());
		if (!band)
			return LF_EGangRadioTune.NO_BAND;

		if (!TuneTo(radio, band))
			return LF_EGangRadioTune.TUNE_FAILED;

		LF_Debug.Info(LF_GangRadio.FEATURE, string.Format("radio tuned org=%1 member=%2 frequency=%3", org.GetId(), characterId, band.m_iFrequency));
		return LF_EGangRadioTune.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a member left. With members left, the gang gets a new band (frequency and key) and the
	//! online members' radios follow; the leaver's radios come off the old band either way. An emptied
	//! gang's band is released by LF_GangRadio itself. \return The new band, null when none was drawn.
	static LF_GangRadioBand OnMemberLeft(string orgId, string leaverCharacterId, string reason)
	{
		LF_GangRadioBand held = LF_GangRadio.Find(orgId);
		if (!held)
			return null;

		int oldFrequency = held.m_iFrequency;
		string oldKey = held.m_sKey;

		array<BaseRadioComponent> radios = {};
		LF_ServiceRadio.FindCarried(LF_OrgReplication.FindOnline(leaverCharacterId), radios);
		Retune(radios, oldFrequency, oldKey, null);

		LF_Organization org = LF_OrganizationManager.Get(orgId);
		if (!org || org.GetMemberCount() <= 0)
			return null;

		LF_GangRadio.Release(orgId, "rekey: " + reason);
		LF_GangRadioBand fresh = LF_GangRadio.BandFor(orgId);
		if (!fresh)
			return null;

		int moved = 0;
		LF_OrganizationRecord snapshot = org.ToRecord();
		if (snapshot && snapshot.m_aMembers)
		{
			foreach (LF_OrganizationMemberRecord member : snapshot.m_aMembers)
			{
				if (!member)
					continue;

				radios.Clear();
				LF_ServiceRadio.FindCarried(LF_OrgReplication.FindOnline(member.m_sCharacterId), radios);
				moved += Retune(radios, oldFrequency, oldKey, fresh);
			}
		}

		LF_Debug.Info(LF_GangRadio.FEATURE, string.Format("band redrawn org=%1 after %2 of %3: frequency %4 -> %5, %6 radio(s) moved", orgId, reason, leaverCharacterId, oldFrequency, fresh.m_iFrequency, moved));
		return fresh;
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber: a member left, so the gang's band is redrawn.
class LF_GangRadioMemberLeftListener : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnOrganizationMemberLeft left = LF_OnOrganizationMemberLeft.Cast(busEvent);
		if (!left)
			return false;

		LF_GangRadioTune.OnMemberLeft(left.m_sOrgId, left.m_sCharacterId, left.m_sReason);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! "Tune to gang channel" on a carried gang radio. Shown to its carrier when they are in a gang;
//! the server checks the carrier and the membership again (LF_GangRadioTune.ServerTune).
class LF_GangRadioTuneAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Comms"))
			return false;

		return LF_HandsetCall.IsCarriedBy(GetOwner(), user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		LF_OrgView view = LF_OrgReplication.ViewOf(user);
		if (!view || view.m_sOrgId.IsEmpty())
		{
			SetCannotPerformReason("#LF-GangRadio_NotMember");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Comms", "LF_GangRadioTuneAction"))
			return;

		if (!Replication.IsServer())
			return;

		LF_GangRadioTune.ServerTune(pUserEntity, pOwnerEntity);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_GangRadioTune's statics (LF_StaticReset finds it by reflection).
class LF_GangRadioTune_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_GangRadioTune";
	}

	override void Apply()
	{
		LF_GangRadioTune.ResetStatics();
	}
}
