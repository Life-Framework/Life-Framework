# Player FAQ

Short answers for players on the Life Framework server. For the download and the first join, see [Open the playtest](playtest.md). For clips and appeals, see [Record a clip](player-clips.md).

## Joining

**How do I get the game?**
Press **Download the playtest** on [life-framework.org/playtest](https://life-framework.org/playtest). Unzip the file anywhere and double-click **Play Life Framework**. It finds Arma Reforger through Steam. Pick **Play on my own** to host the town yourself, or **Join the community server** to join ours.

**Do I need to install anything else?**
No. The Life Framework download holds the whole mod. See [Do I need other mods?](#do-i-need-other-mods) below.

**Why is the server not listed?**
The community server runs on one PC, not around the clock. It is up only in announced sessions. Session times are posted on [Discord](https://discord.gg/MjMexJteqz). The server may also be full.

**What is my character?**
You start as a civilian. On your first join, fill in form R-89 with your name and age. Your character uses your Steam name. Join with the same name and you keep your cash, bank balance, job, organization, and cars.

**Will my progress be wiped?**
Maybe. The server can be wiped after a milestone. Staff announce a wipe a session ahead and back up the save first.

## Controls

| Action | Key |
| --- | --- |
| Use a sign, counter, or door, then pick the row you want | F |
| Lock or unlock a car | U (gamepad: press the right stick) |
| Open the emote menu | F1 |
| Give up while downed (hold it for 30 seconds first) | Hold X (gamepad: hold Y) |
| Type a number on a keypad | 0 to 9, on the number row or the numpad |
| Delete the last digit on a keypad | Backspace |
| Clear a keypad | Delete |

Walking, driving, and the inventory use the normal Arma Reforger controls.

## Money and the bank

**How much money do I start with?**
You start with $1,000 in the bank.

**Where is my money?**
Cash is what you carry. The bank holds the rest. Deposit and withdraw at the bank teller: go in through the bank door and talk to the teller behind the glass.

**What costs money?**
The shop and the car lot take cash. A payphone takes one coin from the cash you carry. The hospital sends you a bill for treatment. Pay it at the hospital's duty desk.

## Jobs

**How do I get a job?**
Sign up at the post office counter. Your first job is parcel delivery. Each parcel run pays $40. Five parcel runs open fuel delivery. Fifteen open the taxi.

**How do I work as a police officer or medic?**
Clock in at the duty desk. Police use the desk at the police station. Medics use the hospital's duty desk. Clocking in swaps your civilian kit for the job kit. Your civilian inventory is kept until you clock out. Clock out at the same desk to swap back. Duty pay is handed over in cash when you clock out.

## Police and crime

**What makes me wanted?**
Crime raises your wanted level. Robbing the fuel station till adds 1. Stealing a weapon adds 2. Stealing a car by force or killing someone adds 3. Your wanted card shows your level.

**What happens when I am arrested?**
An officer arrests you and you go to the cell. In the cell you can work off part of your sentence. Sort the laundry or scrub the floor, and hold still for a few seconds. Each job takes 20 or 30 seconds off. A repeat of the same job waits out a cooldown first.

**Where do I report a crime?**
Use a payphone and pick *Report a crime*, *Report an emergency*, or *Report someone suspicious*. An on-duty officer hears it on the radio and sees it on the wanted board. You can call once a minute. If nobody is on duty, nothing connects and no coin is taken.

**Where do I buy crime tools?**
The fence, Vince, sells them behind the old barn north-west of the bank. He sells only to people with a record of crimes. A drill comes after 3 completed crimes, a breaching charge after 5, and a keycard after 8.

## Dying and the medic

**What happens when I get shot?**
A lethal hit knocks you down. You do not die. You cannot move or act, and you take no more damage. Other players can search you.

**How long do I have?**
Three minutes. You bleed out after that unless someone helps. A bandage, a tourniquet, or a saline bag adds time. Morphine adds no time.

**Can I give up?**
Yes. Hold X (gamepad: Y) after 30 seconds. Giving up counts as death. Disconnecting while downed also counts as death.

**Can a medic save me?**
Yes. A medic on duty can revive a downed player.

## Do I need other mods?

No. The server needs no external mods. The only mod is the Life Framework item, which the launcher installs from the download. Once the Workshop item is listed, the Workshop installs the same item. Players do not install anything else.

Why:

- The mod is one addon. Its only dependency is the base game.
- Every class in the mod uses the `LF_` prefix. The real EveronLife mod uses `EL_`, and it can run on the same server. Using one prefix keeps the two from clashing.
- The project has not built a bridge to other mods. Compatibility means "do not collide", nothing more. A bridge would add risk and no proven benefit.

Server owners: list only the Life Framework item in `game.mods`. See [Install a server](owner/install.md).

## Reporting bugs

**Where do I report a bug?**
Post in **#playtest-feedback** on [Discord](https://discord.gg/MjMexJteqz). Use the pinned template. Staff turn reports into GitHub issues. You can also post a general bug in **#feedback** with the *Bug* tag.

**What should I include?**
- What you did, step by step.
- What you expected, and what happened.
- The server name and the real time, with your time zone.
- The names of the other players involved.
- A clip, if you have one. See [Record a clip](player-clips.md).

**What is /report?**
Type `/report` in chat right after an incident you want staff to see. It flags your recent incidents from the last 10 minutes, including incidents where you were the other party. You can send it once every 2 minutes. Staff see the flag on the Game Master list.
