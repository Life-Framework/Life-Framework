// LF_GarageSlots.c — how many vehicles one character may keep in a garage (#2655, spec #774).
//
// Every character gets LF_GarageConfig.m_iBaseSlots free slots and may buy more up to
// m_iMaxSlots. Slots in use are counted from the stored vehicle records. Bought slots arrive with
// the buy action (G2): until then BoughtSlots reads 0. The price of slot index n (0-based, so
// index m_iBaseSlots is the first slot for sale) is start + step * (n - base).

class LF_GarageSlots
{
	//------------------------------------------------------------------------------------------------
	//! \return Price of the slot at 0-based index; the base slots cost nothing.
	static int PriceOfSlot(int index, int baseSlots, int priceStart, int priceStep)
	{
		if (index < baseSlots)
			return 0;

		return priceStart + priceStep * (index - baseSlots);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Slots a character may use: base plus bought, never past maxSlots.
	static int CapFor(int baseSlots, int boughtSlots, int maxSlots)
	{
		int cap = baseSlots + boughtSlots;
		if (cap > maxSlots)
			cap = maxSlots;

		return cap;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Price of the next slot for a character who owns `slotsOwned` slots, or -1 at the max.
	static int NextSlotPrice(int slotsOwned, int baseSlots, int maxSlots, int priceStart, int priceStep)
	{
		if (slotsOwned >= maxSlots)
			return -1;

		return PriceOfSlot(slotsOwned, baseSlots, priceStart, priceStep);
	}

	//------------------------------------------------------------------------------------------------
	static bool CanBuyNext(int slotsOwned, int maxSlots)
	{
		return slotsOwned < maxSlots;
	}

	//------------------------------------------------------------------------------------------------
	//! Slots bought by ownerUid. 0 until the buy action lands (G2).
	static int BoughtSlots(string ownerUid)
	{
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The slot cap of ownerUid.
	static int CapOf(string ownerUid)
	{
		return CapFor(LF_GarageConfig.BaseSlots(), BoughtSlots(ownerUid), LF_GarageConfig.MaxSlots());
	}

	//------------------------------------------------------------------------------------------------
	//! \return Slots in use: the vehicles ownerUid has stored in a garage.
	static int UsedBy(string ownerUid)
	{
		if (ownerUid.IsEmpty())
			return 0;

		return LF_VehicleOwnership.StoredFor(ownerUid).Count();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Price of ownerUid's next slot, or -1 at the max.
	static int NextPriceFor(string ownerUid)
	{
		return NextSlotPrice(CapOf(ownerUid), LF_GarageConfig.BaseSlots(), LF_GarageConfig.MaxSlots(), LF_GarageConfig.SlotPriceStart(), LF_GarageConfig.SlotPriceStep());
	}
}
