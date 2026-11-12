//------------------------------------------------------------------------------------------------
//! Telephones in the world (#147). A prop that is a telephone carries LF_PhoneComponent; the
//! payphone and the station telephone are the two shipped kinds. The prop itself is not
//! replicated, so each phone gets a line: a small replicated entity (Prefabs/Comms/LF_PhoneLine.et)
//! the server spawns at the phone the first time a call uses it. The line carries what the people
//! at the phone need to see: its state (idle, ringing, busy) and who talks there. The actions read
//! it to show only what applies, and every client rings a RINGING line itself, so a player who
//! walks up or joins mid-ring hears it.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Comms", description: "A telephone: can place and take calls. A station telephone names the service it answers.")]
class LF_PhoneComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_PhoneComponent : ScriptComponent
{
	static const ResourceName LINE_PREFAB = "{F5BCA5ED11FDC89D}Prefabs/Comms/LF_PhoneLine.et";

	[Attribute("", UIWidgets.EditBox, "Service key (Configs/Comms/LF_ServiceCalls.conf) this station telephone answers, e.g. police. Empty on a payphone")]
	protected string m_sDispatchService;

	//! Every phone on this machine (server and clients alike).
	protected static ref array<LF_PhoneComponent> s_aPhones;

	protected EntityID m_Line = EntityID.INVALID;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aPhones = null;
	}

	//------------------------------------------------------------------------------------------------
	static array<LF_PhoneComponent> Phones()
	{
		if (!s_aPhones)
			s_aPhones = {};

		return s_aPhones;
	}

	//------------------------------------------------------------------------------------------------
	static LF_PhoneComponent Of(IEntity phone)
	{
		if (!phone)
			return null;

		return LF_PhoneComponent.Cast(phone.FindComponent(LF_PhoneComponent));
	}

	//------------------------------------------------------------------------------------------------
	static LF_PhoneComponent ByKey(string key)
	{
		if (key.IsEmpty())
			return null;

		foreach (LF_PhoneComponent phone : Phones())
		{
			if (phone && phone.Key() == key)
				return phone;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The keys of every station telephone that answers this service.
	static void DispatchPhones(string service, notnull array<string> outKeys)
	{
		outKeys.Clear();
		if (service.IsEmpty())
			return;

		foreach (LF_PhoneComponent phone : Phones())
		{
			if (phone && phone.DispatchService() == service)
				outKeys.Insert(phone.Key());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Stable name of this phone: where it stands, to the metre (the payphone cooldown key).
	string Key()
	{
		return LF_Payphone.PhoneKey(GetOwner().GetOrigin());
	}

	//------------------------------------------------------------------------------------------------
	string DispatchService()
	{
		return m_sDispatchService;
	}

	//------------------------------------------------------------------------------------------------
	bool IsDispatchPhone()
	{
		return !m_sDispatchService.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	string Place()
	{
		return LF_WantedReports.DescribePlace(GetOwner().GetOrigin());
	}

	//------------------------------------------------------------------------------------------------
	//! This phone's line on this machine, or null before the server spawned it (or before it
	//! streamed in on a client).
	LF_PhoneLineComponent Line()
	{
		return LF_PhoneLineComponent.ForPhone(Key());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: this phone's line, spawned on first use.
	LF_PhoneLineComponent ServerLine()
	{
		if (!Replication.IsServer())
			return null;

		IEntity line;
		if (m_Line != EntityID.INVALID && GetGame().GetWorld())
			line = GetGame().GetWorld().FindEntityByID(m_Line);

		if (!line)
		{
			line = LF_Utils.SpawnEntityPrefab(LINE_PREFAB, GetOwner().GetOrigin() + LF_PhoneLineComponent.LINE_OFFSET);
			if (!line)
			{
				LF_Debug.Error(LF_PhoneCalls.FEATURE, string.Format("line prefab did not spawn at phone %1; this phone cannot ring", Key()));
				return null;
			}

			m_Line = line.GetID();
			LF_Debug.Info(LF_PhoneCalls.FEATURE, string.Format("line spawned for phone %1 (%2)", Key(), Place()));
		}

		LF_PhoneLineComponent component = LF_PhoneLineComponent.Cast(line.FindComponent(LF_PhoneLineComponent));
		if (!component)
		{
			LF_Debug.Error(LF_PhoneCalls.FEATURE, string.Format("line at phone %1 has no LF_PhoneLineComponent: %2", Key(), LINE_PREFAB));
			return null;
		}

		component.ServerBind(Key());
		return component;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the line entity, when one was spawned. Never spawns.
	IEntity ServerLineEntity()
	{
		if (m_Line == EntityID.INVALID || !GetGame().GetWorld())
			return null;

		return GetGame().GetWorld().FindEntityByID(m_Line);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		Phones().Insert(this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_aPhones)
			s_aPhones.RemoveItem(this);

		// A destroyed phone takes its line with it; the call table notices on its next tick.
		IEntity line = ServerLineEntity();
		if (line && Replication.IsServer())
			SCR_EntityHelper.DeleteEntityAndChildren(line);
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Comms", description: "The replicated line of one telephone: its state and who talks there. Spawned by the server.")]
class LF_PhoneLineComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_PhoneLineComponent : ScriptComponent
{
	//! Where the line stands relative to its phone: at receiver height.
	static const vector LINE_OFFSET = "0 1.2 0";

	[RplProp(onRplName: "OnLineReplicated")]
	protected string m_sPhoneKey;

	[RplProp(onRplName: "OnLineReplicated")]
	protected int m_iState;

	//! Server player id of the person talking at this phone; 0 for nobody. A player id, never an
	//! EntityID, so it means the same on every machine.
	[RplProp(onRplName: "OnLineReplicated")]
	protected int m_iTalkerId;

	//! Player id of the caller who may leave a call-back request at this idle phone; 0 for nobody.
	[RplProp()]
	protected int m_iOfferId;

	protected static ref array<LF_PhoneLineComponent> s_aLines;
	protected bool m_bRinging;
	protected int m_iRings;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aLines = null;
	}

	//------------------------------------------------------------------------------------------------
	static array<LF_PhoneLineComponent> Lines()
	{
		if (!s_aLines)
			s_aLines = {};

		return s_aLines;
	}

	//------------------------------------------------------------------------------------------------
	//! The line of the phone with this key on this machine, or null. Read at query time: a
	//! client gets the key with the line's first snapshot, after the component registered.
	static LF_PhoneLineComponent ForPhone(string phoneKey)
	{
		if (phoneKey.IsEmpty())
			return null;

		foreach (LF_PhoneLineComponent line : Lines())
		{
			if (line && line.m_sPhoneKey == phoneKey)
				return line;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Client side of the actions: the state of the line at this phone; IDLE when it has no line.
	static LF_EPhoneLineState StateAt(IEntity phone, out int talkerId)
	{
		talkerId = 0;
		LF_PhoneComponent component = LF_PhoneComponent.Of(phone);
		if (!component)
			return LF_EPhoneLineState.IDLE;

		LF_PhoneLineComponent line = component.Line();
		if (!line)
			return LF_EPhoneLineState.IDLE;

		talkerId = line.m_iTalkerId;
		return line.GetState();
	}

	//------------------------------------------------------------------------------------------------
	LF_EPhoneLineState GetState()
	{
		return m_iState;
	}

	//------------------------------------------------------------------------------------------------
	int GetTalkerId()
	{
		return m_iTalkerId;
	}

	//------------------------------------------------------------------------------------------------
	int GetOfferId()
	{
		return m_iOfferId;
	}

	//------------------------------------------------------------------------------------------------
	//! Client side of the leave action: who holds a call-back offer at this phone; 0 for nobody.
	static int OfferAt(IEntity phone)
	{
		LF_PhoneComponent component = LF_PhoneComponent.Of(phone);
		if (!component)
			return 0;

		LF_PhoneLineComponent line = component.Line();
		if (!line)
			return 0;

		return line.m_iOfferId;
	}

	//------------------------------------------------------------------------------------------------
	string GetPhoneKey()
	{
		return m_sPhoneKey;
	}

	//------------------------------------------------------------------------------------------------
	//! Rings heard on this machine since the line came up, for the harness and the proofs.
	int GetRings()
	{
		return m_iRings;
	}

	//------------------------------------------------------------------------------------------------
	//! Every line on this machine as "key:STATE:talker:rings:offer", for the harness `phone` verb.
	static string DescribeAll()
	{
		string text = string.Format("lines=%1", Lines().Count());
		foreach (LF_PhoneLineComponent line : Lines())
		{
			if (line)
				text += string.Format(" %1:%2:%3:%4:%5", line.m_sPhoneKey, typename.EnumToString(LF_EPhoneLineState, line.m_iState), line.m_iTalkerId, line.m_iRings, line.m_iOfferId);
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: name the phone this line belongs to (once, at spawn).
	void ServerBind(string phoneKey)
	{
		if (!Replication.IsServer() || m_sPhoneKey == phoneKey)
			return;

		m_sPhoneKey = phoneKey;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: show the call table's view of this phone. Bumps only on a change.
	void ServerSet(LF_EPhoneLineState state, int talkerId, int offerId = 0)
	{
		if (!Replication.IsServer())
			return;

		if (m_iState == state && m_iTalkerId == talkerId && m_iOfferId == offerId)
			return;

		m_iState = state;
		m_iTalkerId = talkerId;
		m_iOfferId = offerId;
		Replication.BumpMe();
		// A listen server's own player hears the ring too; RplProp handlers do not run on the server.
		OnLineReplicated();
	}

	//------------------------------------------------------------------------------------------------
	//! A line that starts ringing rings at once; the pulse keeps it ringing.
	protected void OnLineReplicated()
	{
		bool ringing = m_iState == LF_EPhoneLineState.RINGING;
		if (ringing && !m_bRinging)
			Ring();

		m_bRinging = ringing;
	}

	//------------------------------------------------------------------------------------------------
	//! Every rendering machine checks each line it has on its own clock and rings a RINGING one.
	//! Reading the state here, not trusting the replication handler alone, gives a late joiner and
	//! a line that streams in mid-ring the ring with no extra message.
	protected void Pulse()
	{
		if (m_iState == LF_EPhoneLineState.RINGING)
		{
			m_bRinging = true;
			Ring();
		}
		else
		{
			m_bRinging = false;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One ring at the phone. A dedicated server renders and hears nothing.
	protected void Ring()
	{
		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		IEntity at = GetOwner();
		LF_PhoneComponent phone = LF_PhoneComponent.ByKey(m_sPhoneKey);
		if (phone)
			at = phone.GetOwner();

		ResourceName project = LF_PhoneConfig.RingSoundProject();
		string soundEvent = LF_PhoneConfig.RingSoundEvent();
		if (!at || project.IsEmpty() || soundEvent.IsEmpty())
			return;

		SCR_AudioSourceConfiguration config = new SCR_AudioSourceConfiguration();
		config.m_sSoundProject = project;
		config.m_sSoundEventName = soundEvent;
		SCR_SoundManagerModule.CreateAndPlayAudioSource(at, config);
		m_iRings++;
		if (m_iRings == 1)
			LF_Debug.Info(LF_PhoneCalls.FEATURE, string.Format("phone %1 rings on this machine (at the phone prop=%2)", m_sPhoneKey, phone != null));
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		Lines().Insert(this);
		if (RplSession.Mode() != RplMode.Dedicated && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(Pulse, LF_PhoneConfig.RingPulseSeconds() * 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_aLines)
			s_aLines.RemoveItem(this);

		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Pulse);
	}
}
