// Ported from thebonbon/EveronLife (MIT), enhancement/Main_World:
// EL_DestructionMultiPhaseSingleComponent.c and EL_ToolHitZone.c.
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
//! One damage step on a minable ore. Health is the phase budget. A named mesh
//! replaces the ore mesh. An empty mesh leaves the current one in place.
class LF_OrePhase
{
	float m_fHealth;
	ResourceName m_Model;
}

//------------------------------------------------------------------------------------------------
//! Live ore the phase helpers mutate. The test drives this with no world boot.
class LF_OreNode
{
	float m_fHealth;
	float m_fOriginalHealth;
	int m_iPhase;
	ResourceName m_OriginalModel;
	ResourceName m_Model;
	bool m_bSwapMesh;
	bool m_bDestroyed;
	float m_fEffectiveDamage;
	bool m_bDeleteAfterFinal;
	ref array<ref LF_OrePhase> m_aPhases;
}

//------------------------------------------------------------------------------------------------
//! Ore health phases. A tool hit is accepted only when its damage equals the
//! configured tool damage. Accepted damage walks phase budgets the way
//! SCR_DestructionMultiPhaseComponent.CalculateDamagePhase does, then applies
//! the Bonbon phase: health becomes that phase's budget and the mesh swaps
//! when the phase names one. No phase data restores the original health and mesh.
class LF_MiningPhases
{
	//------------------------------------------------------------------------------------------------
	//! Base health plus every phase budget. That sum is the intact ore's health.
	static float OriginalHealth(float baseHealth, array<ref LF_OrePhase> phases)
	{
		float total = baseHealth;
		if (!phases)
			return total;

		foreach (LF_OrePhase phase : phases)
		{
			if (phase)
				total = total + phase.m_fHealth;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Initial phase plus one slot per configured phase.
	static int PhaseCount(array<ref LF_OrePhase> phases)
	{
		if (!phases)
			return 1;

		return phases.Count() + 1;
	}

	//------------------------------------------------------------------------------------------------
	static int ClampPhase(int phase, int maxPhase)
	{
		if (phase < 0)
			return 0;

		if (phase > maxPhase)
			return maxPhase;

		return phase;
	}

	//------------------------------------------------------------------------------------------------
	//! Phase index after one hit of `damage`, starting from `currentPhase`.
	//! Negative damage stays put. Any non-negative damage enters the walk, spends
	//! each phase budget, and stops on the phase the remainder cannot finish.
	//! A budget of zero or less stops the walk so a bad row cannot spin.
	//! deleteAfterFinal allows the index one past the last phase (destroyed).
	static int PhaseAfterDamage(int currentPhase, array<ref LF_OrePhase> phases, float damage, bool deleteAfterFinal)
	{
		if (damage < 0)
			return currentPhase;

		int phaseCount = PhaseCount(phases);
		int damagePhase = currentPhase;
		float remaining = damage;
		while (remaining >= 0)
		{
			damagePhase++;
			int index = damagePhase - 1;
			LF_OrePhase phaseData = null;
			if (phases && index >= 0 && index < phases.Count())
				phaseData = phases[index];

			if (!phaseData)
				break;

			if (phaseData.m_fHealth <= 0)
				break;

			remaining = remaining - phaseData.m_fHealth;
		}

		int maxPhase = phaseCount - 1;
		if (deleteAfterFinal)
			maxPhase = phaseCount;

		return ClampPhase(damagePhase, maxPhase);
	}

	//------------------------------------------------------------------------------------------------
	//! Bonbon ApplyDamagePhaseData. Phase 0 and any index with no row restore
	//! the original health and mesh. A named phase mesh is swapped in. An empty
	//! phase mesh keeps `currentModel` and still sets health to the phase budget.
	static float ApplyPhase(int phaseIndex, float originalHealth, ResourceName originalModel, array<ref LF_OrePhase> phases, ResourceName currentModel, out ResourceName model, out bool swapMesh)
	{
		LF_OrePhase phase = null;
		if (phases && phaseIndex > 0)
		{
			int index = phaseIndex - 1;
			if (index < phases.Count())
				phase = phases[index];
		}

		if (!phase)
		{
			model = originalModel;
			swapMesh = true;
			return originalHealth;
		}

		if (phase.m_Model.IsEmpty())
		{
			model = currentModel;
			swapMesh = false;
			return phase.m_fHealth;
		}

		model = phase.m_Model;
		swapMesh = true;
		return phase.m_fHealth;
	}

	//------------------------------------------------------------------------------------------------
	//! EL_ToolHitZone: damage passes only when it equals the tool's effective damage.
	static float ToolDamage(float rawDamage, float effectiveDamage)
	{
		if (rawDamage != effectiveDamage)
			return 0;

		return rawDamage;
	}

	//------------------------------------------------------------------------------------------------
	//! One strike. A mismatched tool does nothing. A finishing blow destroys the
	//! ore when delete-after-final is set. Otherwise the new phase's health and
	//! mesh are applied.
	static void Strike(LF_OreNode node, float rawDamage)
	{
		if (!node)
		{
			EL_Debug.Error("Mining", "strike ignored: no ore");
			return;
		}

		node.m_bSwapMesh = false;
		if (node.m_bDestroyed)
		{
			EL_Debug.Log("Mining", "strike ignored: ore is already destroyed");
			return;
		}

		float dealt = ToolDamage(rawDamage, node.m_fEffectiveDamage);
		if (dealt <= 0)
		{
			EL_Debug.Log("Mining", "strike ignored: tool damage does not match");
			return;
		}

		int next = PhaseAfterDamage(node.m_iPhase, node.m_aPhases, dealt, node.m_bDeleteAfterFinal);
		int count = PhaseCount(node.m_aPhases);
		if (node.m_bDeleteAfterFinal && next >= count)
		{
			node.m_iPhase = next;
			node.m_fHealth = 0;
			node.m_bDestroyed = true;
			EL_Debug.Log("Mining", string.Format("ore destroyed at phase %1", next));
			return;
		}

		if (next == node.m_iPhase)
			return;

		ResourceName nextModel = node.m_Model;
		bool swapMesh = false;
		node.m_fHealth = ApplyPhase(next, node.m_fOriginalHealth, node.m_OriginalModel, node.m_aPhases, node.m_Model, nextModel, swapMesh);
		node.m_Model = nextModel;
		node.m_bSwapMesh = swapMesh;
		node.m_iPhase = next;
		EL_Debug.Log("Mining", string.Format("ore phase %1 health %2", next, node.m_fHealth));
	}
}
