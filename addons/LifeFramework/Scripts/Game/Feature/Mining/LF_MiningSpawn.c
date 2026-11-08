// Ported from thebonbon/EveronLife (MIT), enhancement/Main_World:
// EL_MiningArea.c weighted ore spawn.
// Copyright (c) 2022 EveronLife
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//------------------------------------------------------------------------------------------------
//! One weighted ore prefab. Chance is the width of its bin.
class LF_OreWeight
{
	int m_iChance;
	ResourceName m_Prefab;
}

//------------------------------------------------------------------------------------------------
//! Weighted ore pick. A roll of 0 lands in the first bin. The spawn count is
//! the shortfall to the configured amount and is never negative.
class LF_MiningSpawn
{
	//------------------------------------------------------------------------------------------------
	static int TotalWeight(array<ref LF_OreWeight> weights)
	{
		int totalWeight = 0;
		if (!weights)
			return totalWeight;

		foreach (LF_OreWeight entry : weights)
		{
			if (entry)
				totalWeight = totalWeight + entry.m_iChance;
		}

		return totalWeight;
	}

	//------------------------------------------------------------------------------------------------
	//! Maps one roll in [0, total) onto a weighted entry. Returns -1 when the
	//! list is empty, sums to zero, or the roll falls outside the total.
	static int PickIndex(array<ref LF_OreWeight> weights, int roll)
	{
		if (!weights || weights.Count() == 0)
		{
			EL_Debug.Error("Mining", "ore spawn skipped: no weights configured");
			return -1;
		}

		int totalWeight = TotalWeight(weights);
		if (totalWeight <= 0)
		{
			EL_Debug.Error("Mining", "ore spawn skipped: weights sum to zero");
			return -1;
		}

		int remaining = roll;
		for (int i = 0; i < weights.Count(); i++)
		{
			LF_OreWeight entry = weights[i];
			if (!entry)
				continue;

			if (remaining < entry.m_iChance)
				return i;

			remaining = remaining - entry.m_iChance;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! How many ores a respawn may still place.
	static int SpawnCount(int amount, int existing)
	{
		int count = amount - existing;
		if (count < 0)
			return 0;

		return count;
	}
}
