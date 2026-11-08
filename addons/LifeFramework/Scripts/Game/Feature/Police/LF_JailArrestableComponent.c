// Ported from LNGCLifeRP_Public (MIT), author LNGCGenetic.
// Copyright (c) 2026 LNGCGenetic
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
//! Character that can be escorted. Hosts the escort tick. The tick does not
//! start or advance a sentence; arrival at the cell does that.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Police", description: "Escort tick for a character an on-duty officer can walk to a vehicle. The sentence starts at the cell.")]
class LF_JailArrestableComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_JailArrestableComponent : ScriptComponent
{
	protected static const int TICK_MS = 1000;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
			return;

		if (!owner)
		{
			EL_Debug.Error("Jail", "arrestable component has no owner");
			return;
		}

		if (!GetGame() || !GetGame().GetCallqueue())
		{
			EL_Debug.Error("Jail", "escort tick skipped: no callqueue");
			return;
		}

		GetGame().GetCallqueue().CallLater(OnTick, TICK_MS, true);
		EL_Debug.Log("Jail", "escort tick scheduled");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnTick()
	{
		if (!Replication.IsServer())
			return;

		LF_JailEscort.Tick(1.0);
	}
}
