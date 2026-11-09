// Author: LNGCGenetic
// Copyright (c) LNGCGenetic
// Permission is hereby granted, free of charge, to any person obtaining a copy of this
// software and associated documentation files (the "Software"), to deal in the Software
// without restriction, including without limitation the rights to use, copy, modify,
// merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following
// conditions:
// The above copyright notice and this permission notice shall be included in all copies
// or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.
// Ported from LNGC_GarageDialog.
// That dialog was a picker over stored vehicles plus the lot catalog. The lot sells, so only
// the owner's rows remain: they feed the garage ledger card (LF_GarageLedger).
//------------------------------------------------------------------------------------------------
class LF_VehicleGarageRow : Managed
{
	string m_sVehicleId;
	string m_sLabelKey;
	string m_sPlate;
	//! LF_GarageLedger.PLACE_*: in the garage, on the street, or at the impound lot.
	int m_iPlace;
}

//------------------------------------------------------------------------------------------------
class LF_VehicleGarageDialog
{
	//------------------------------------------------------------------------------------------------
	static array<ref LF_VehicleGarageRow> StoredRows(string ownerUid)
	{
		array<ref LF_VehicleGarageRow> rows = new array<ref LF_VehicleGarageRow>();
		AppendRows(rows, LF_VehicleOwnership.StoredFor(ownerUid), LF_GarageLedger.PLACE_STORED);
		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Stored vehicles first, then the ones out on the street, then the ones at the impound lot.
	static array<ref LF_VehicleGarageRow> LedgerRows(string ownerUid)
	{
		array<ref LF_VehicleGarageRow> rows = new array<ref LF_VehicleGarageRow>();
		AppendRows(rows, LF_VehicleOwnership.StoredFor(ownerUid), LF_GarageLedger.PLACE_STORED);
		AppendRows(rows, LF_VehicleOwnership.InWorldFor(ownerUid), LF_GarageLedger.PLACE_STREET);
		AppendRows(rows, LF_VehicleOwnership.ImpoundedFor(ownerUid), LF_GarageLedger.PLACE_IMPOUNDED);
		return rows;
	}

	//------------------------------------------------------------------------------------------------
	protected static void AppendRows(array<ref LF_VehicleGarageRow> rows, array<ref LF_VehicleRecord> records, int place)
	{
		if (!records)
			return;

		foreach (LF_VehicleRecord record : records)
		{
			if (!record)
				continue;

			LF_VehicleGarageRow row = new LF_VehicleGarageRow();
			row.m_sVehicleId = record.m_sVehicleId;
			row.m_sLabelKey = record.m_sNameKey;
			row.m_sPlate = record.m_sPlate;
			row.m_iPlace = place;
			rows.Insert(row);
		}
	}
}
