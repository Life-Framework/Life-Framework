// Ported from thebonbon/EveronLife (MIT), enhancement/Main_World:
// EL_ProcessAction timed process on the ore forge (nugget to bar).
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
//! One forge heat. The nugget stays until the delay elapses, then it becomes a bar.
class LF_ForgeHeat
{
	ResourceName m_Nugget;
	ResourceName m_Bar;
	int m_iNuggetsRequired;
	int m_iBarsYielded;
	int m_iDurationMs;
	int m_iElapsedMs;
	bool m_bHeating;
	int m_iNuggets;
	int m_iBars;
}

//------------------------------------------------------------------------------------------------
//! Timed forge. Bonbon's ore processor waits m_fProcessTime seconds
//! (CallLater delay = seconds * 1000) and then turns one iron nugget into one iron bar.
class LF_MiningForge
{
	static const int IRON_NUGGETS = 1;
	static const int IRON_BARS = 1;
	static const float IRON_SECONDS = 5;

	static const ResourceName IRON_NUGGET = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";
	static const ResourceName IRON_BAR = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";

	//------------------------------------------------------------------------------------------------
	//! CallLater delay in milliseconds. Zero and negative seconds wait 0 ms.
	static int DelayMs(float seconds)
	{
		if (seconds <= 0)
			return 0;

		return seconds * 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! Iron row from the ore processor: 1 nugget, 1 bar, 5 seconds.
	static void UseIronRecipe(LF_ForgeHeat heat)
	{
		if (!heat)
		{
			EL_Debug.Error("Mining", "forge recipe rejected: no heat");
			return;
		}

		heat.m_Nugget = IRON_NUGGET;
		heat.m_Bar = IRON_BAR;
		heat.m_iNuggetsRequired = IRON_NUGGETS;
		heat.m_iBarsYielded = IRON_BARS;
		heat.m_iDurationMs = DelayMs(IRON_SECONDS);
	}

	//------------------------------------------------------------------------------------------------
	static bool RecipeReady(LF_ForgeHeat heat)
	{
		if (!heat)
			return false;

		if (heat.m_Nugget.IsEmpty() || heat.m_Bar.IsEmpty())
			return false;

		if (heat.m_iNuggetsRequired < 1 || heat.m_iBarsYielded < 1)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Consumes the nugget and yields the bar. The heat is no longer running.
	//! \return True when a bar was produced.
	static bool Finish(LF_ForgeHeat heat)
	{
		heat.m_bHeating = false;
		heat.m_iElapsedMs = 0;
		if (heat.m_iNuggets < heat.m_iNuggetsRequired)
		{
			EL_Debug.Error("Mining", "forge failed: nugget missing when the heat finished");
			return false;
		}

		heat.m_iNuggets = heat.m_iNuggets - heat.m_iNuggetsRequired;
		heat.m_iBars = heat.m_iBars + heat.m_iBarsYielded;
		EL_Debug.Log("Mining", string.Format("forge finished %1 -> %2", heat.m_Nugget, heat.m_Bar));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the heat when a nugget is held and no heat is already running.
	//! A zero delay finishes before Start returns.
	static bool Start(LF_ForgeHeat heat)
	{
		if (!heat)
		{
			EL_Debug.Error("Mining", "forge rejected: no heat");
			return false;
		}

		if (heat.m_bHeating)
		{
			EL_Debug.Error("Mining", "forge rejected: a heat is already running");
			return false;
		}

		if (!RecipeReady(heat))
		{
			EL_Debug.Error("Mining", "forge rejected: nugget or bar is not configured");
			return false;
		}

		if (heat.m_iNuggets < heat.m_iNuggetsRequired)
		{
			EL_Debug.Error("Mining", "forge rejected: nugget missing");
			return false;
		}

		heat.m_bHeating = true;
		heat.m_iElapsedMs = 0;
		EL_Debug.Log("Mining", string.Format("forge started for %1 ms", heat.m_iDurationMs));
		if (heat.m_iDurationMs <= 0)
			return Finish(heat);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves a running heat forward. The nugget is still held until the delay elapses.
	//! \return True when this step produced a bar.
	static bool Advance(LF_ForgeHeat heat, int deltaMs)
	{
		if (!heat || !heat.m_bHeating)
			return false;

		if (deltaMs < 0)
			deltaMs = 0;

		heat.m_iElapsedMs = heat.m_iElapsedMs + deltaMs;
		if (heat.m_iElapsedMs < heat.m_iDurationMs)
			return false;

		return Finish(heat);
	}
}
