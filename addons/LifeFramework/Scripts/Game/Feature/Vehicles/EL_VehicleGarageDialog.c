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
// Ported from LNGC_GarageDialog. That dialog was a picker over stored vehicles plus the
// lot catalog. These rows are that picker. Labels are localization keys.
//------------------------------------------------------------------------------------------------
class EL_VehicleGarageRow : Managed
{
	string m_sVehicleId;
	string m_sLabelKey;
	string m_sPlate;
	int m_iPrice;
}

//------------------------------------------------------------------------------------------------
class EL_VehicleGarageDialog
{
	//------------------------------------------------------------------------------------------------
	static array<ref EL_VehicleGarageRow> CatalogRows(EL_VehicleLotComponent lot)
	{
		array<ref EL_VehicleGarageRow> rows = new array<ref EL_VehicleGarageRow>();
		if (!lot)
			return rows;

		for (int i = 0; i < lot.GetOfferCount(); i++)
		{
			EL_VehicleOffer offer = lot.GetOfferAt(i);
			if (!offer)
				continue;

			EL_VehicleGarageRow row = new EL_VehicleGarageRow();
			row.m_sLabelKey = offer.GetNameKey();
			row.m_iPrice = offer.GetPrice();
			rows.Insert(row);
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	static array<ref EL_VehicleGarageRow> StoredRows(string ownerUid)
	{
		array<ref EL_VehicleGarageRow> rows = new array<ref EL_VehicleGarageRow>();
		array<ref EL_VehicleRecord> stored = EL_VehicleOwnership.StoredFor(ownerUid);
		if (!stored)
			return rows;

		foreach (EL_VehicleRecord record : stored)
		{
			if (!record)
				continue;

			EL_VehicleGarageRow row = new EL_VehicleGarageRow();
			row.m_sVehicleId = record.m_sVehicleId;
			row.m_sLabelKey = record.m_sNameKey;
			row.m_sPlate = record.m_sPlate;
			rows.Insert(row);
		}

		return rows;
	}
}
