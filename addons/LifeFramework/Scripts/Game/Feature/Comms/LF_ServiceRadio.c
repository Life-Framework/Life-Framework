//------------------------------------------------------------------------------------------------
//! Service radios (#252). A duty kit carries a vanilla handheld radio (ANPRC68) and the duty swap
//! tunes its first transceiver to the service's frequency from Configs/Comms/LF_ServiceCalls.conf
//! (`m_iFrequency`, kHz). Vanilla radio rules do the rest: range-limited voice on one frequency,
//! nothing heard off it. A restored inventory is a prefab snapshot, so the tuning is applied again
//! on every clock-in and on a reconnect while on duty.
//------------------------------------------------------------------------------------------------
class LF_ServiceRadio
{
	static const string FEATURE = "Radio";
	static const ResourceName RADIO_PREFAB = "{73950FBA2D7DB5C5}Prefabs/Items/Equipment/Radios/Radio_ANPRC68.et";

	//------------------------------------------------------------------------------------------------
	//! The service key whose frequency a duty role uses; "" for a role with no service.
	static string ServiceFor(LF_Faction role)
	{
		if (role == LF_Faction.POLICE)
			return "police";
		if (role == LF_Faction.MEDIC)
			return "medic";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Every radio in a character's inventory or hands, each once.
	static void FindCarried(IEntity character, notnull array<BaseRadioComponent> outRadios)
	{
		if (!character)
			return;

		array<IEntity> items = {};
		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(character);
		if (inventory)
			inventory.GetItems(items);

		foreach (IEntity item : items)
		{
			if (!item)
				continue;

			BaseRadioComponent radio = BaseRadioComponent.Cast(item.FindComponent(BaseRadioComponent));
			if (radio && outRadios.Find(radio) < 0)
				outRadios.Insert(radio);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: tunes the first transceiver of a radio to a frequency.
	//! \return False (and an Error) when the radio has no transceiver or cannot tune there.
	static bool Tune(notnull BaseRadioComponent radio, int frequency)
	{
		BaseTransceiver transceiver = radio.GetTransceiver(0);
		if (!transceiver)
		{
			LF_Debug.Error(FEATURE, "radio has no transceiver, left untuned");
			return false;
		}

		if (frequency < transceiver.GetMinFrequency() || frequency > transceiver.GetMaxFrequency())
		{
			LF_Debug.Error(FEATURE, string.Format("frequency %1 kHz is outside the radio range %2..%3, left untuned", frequency, transceiver.GetMinFrequency(), transceiver.GetMaxFrequency()));
			return false;
		}

		transceiver.SetFrequency(frequency);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: tunes every radio a duty role carries to the role's service frequency.
	//! \return How many radios were tuned.
	static int TuneForRole(IEntity character, LF_Faction role)
	{
		int frequency = LF_ServiceCallsConfig.Frequency(ServiceFor(role));
		if (frequency <= 0)
			return 0;

		array<BaseRadioComponent> radios = {};
		FindCarried(character, radios);
		int tuned = 0;
		foreach (BaseRadioComponent radio : radios)
		{
			if (Tune(radio, frequency))
				tuned++;
		}

		if (tuned > 0)
			LF_Debug.Info(FEATURE, string.Format("%1 radio(s) tuned to %2 kHz for %3", tuned, frequency, typename.EnumToString(LF_Faction, role)));

		return tuned;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: after an inventory change, an on-duty account's radios sit on their service frequency.
	static int TuneForAccount(LF_PlayerAccount account, IEntity character)
	{
		if (!account || !account.IsOnDuty())
			return 0;

		return TuneForRole(character, account.GetFaction());
	}
}
