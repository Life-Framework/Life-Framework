# 6. Police

Police are players. There is no roster to wait for in the alpha: anyone can sign up at the station and start the same minute.

## Join and go on duty

1. Go to the **police station** on the main street (east of the square; the gun store is next door). Press **F** at the **duty desk** (the punch clock).
2. First time only: pick **Apply**. In the alpha the desk **approves you on the spot**.
3. Pick **Clock in**. Your civilian clothes and pack are **stored** and you swap into the **police kit**: cap, dark turtleneck, trousers, officer harness, boots,
   an M9 pistol with two magazines, the **station car key**, a pager, **handcuffs** and a radio. **Clock out** at the same desk to swap back.
4. If you die on duty your shift ends; you keep the police gear and your civilian things come back when you respawn.
5. **Your pager** beeps with each dispatch call. When a car owner reports their car stolen, it also shows a line with the **plate and the car**. The line stays on the pager until an officer recovers the car or a chop shop scraps it.

**Pay** is handed over in **cash when you clock out**, by the second you were on duty, plus your **bonus**.

| Rank | Pay per hour | Bonus cap per hour | Fine limit | On duty to rise |
|---|---|---|---|---|
| Cadet | $350 | $100 | $500 | 1 hour |
| Patrol | $375 | $100 | $1,000 | 2 hours |
| Senior Patrol | $400 | $100 | $2,500 | 4 hours |
| Sergeant | $425 | $95 | $4,000 | 8 hours |
| Lieutenant | $450 | $70 | $5,000 | top |

**Bonus:** a call closed as resolved (fine the suspect at the scene) earns **$15**, a suspect booked at the desk **$10**, and a stolen car recovered to its owner **$20** (not your own car). The bonus stops at your rank's cap each hour and is paid with your shift when you clock out. It survives a server restart. Fines go to the town, never to you.

A higher rank also promotes: at the **armoury**, an officer above the new rank can promote you. Higher ranks draw bigger weapons (the M16A2, then the AKS-74U).

## The car

Three **police cars** (covered UAZ469 with a blue light bar) stand beside the station. The **station car key** in your kit opens and starts every one of them. Press
**F6** to lock or unlock a car (gamepad: press the right stick). Cycle the siren modes with the **right mouse button** (gamepad: click the left stick).
Station cars are not owned: they are back at the station after a restart.

**Car pool board.** On duty, press **F** at the car pool board and pick your rank's car ("Take the Cadet car": a white S105; "Take the Patrol car": the white UAZ469 with POLICE on the doors, Patrol and up). It stands in the bay in front of the board, unlocked for officers on duty. Taking another car sends your last one back, unless someone is sitting in it. A car above your rank is greyed out with the rank it needs. The board is not at the Saint-Philippe station yet.

## Stop, cuff, search, arrest

An arrest is five steps: **cuff**, **escort**, **put in vehicle**, drive to the station, **book into jail** at the cell.

* **Search:** face a player within 2 m, press **F** to open the player menu and pick **Search**. Every item they carry is checked. Anything illegal for them is **seized and filed as an offence**.
  A pistol with a firearm permit is left alone. You see "Seized from <name>: ..." when something is taken.
* **Search a car:** stand by a stopped car and pick **Search vehicle**. Cartons, bottles and other contraband in the trunk show up by class: "Found in the trunk: 2 tobacco, 1 alcohol". **Search again** within 5 minutes to **seize** them; the people in the car are charged. Legal goods stay in the car.
* **Cuff:** hold the **handcuffs** in your left hand and pick **Cuff** (a 2 second hold) on a player who is **downed, stunned, held up, or has their hands up and no weapon**.
  A cuffed player can walk but cannot hold anything. **Uncuff** frees them; you get the cuffs back.
  Cuffing a **wanted** player puts a **"Jail booking" waypoint** on your screen: that is where you book them.
* **Radar gun:** hold the radar gun, aim at a car (up to 150 m) and pick **Measure speed**. A toast shows the speed and the limit, and the class: **minor** (1 to 15 km/h over), **road** (16 to 30 over) or **serious** (over 30). Each class is a law-book charge. The gun is not issued in the kit yet.
* **Escort:** pick **Escort** on a cuffed suspect to walk them along. An uncuffed suspect cannot be escorted.
* **Put in vehicle:** at a police car pick **Put in vehicle** to seat your suspect, then drive to the station. At the station pick **Take out of vehicle**; they step out beside the car, still yours.
* **Book into jail:** walk the cuffed suspect to the **cell door** (the waypoint). Their action list then shows **Book into jail: full sentence** and
  **Book into jail: reduced (cooperated)** (half, at least 30 seconds). Pick one: they go into the cell, the cuffs come back to you. Booking is only offered at the jail.
* **Booking desk:** stand at the booking desk with the cuffed suspect beside you and pick **Book suspect**. The desk charges them from their open crime reports. The suspect then sees **Pay $X now** and **Take the time (N min)** on the desk and picks one. Paying takes the fine from the cash they carry first and their bank for the rest; the cuffs come off, the charges are cleared and they walk free. The money goes to the town, never to you. A suspect who can't cover the fine, or who takes the time, goes to the cell. A charge with only a fine still costs cell time when refused: a minute for every $60, at most 15 minutes. One booking at a time per suspect.
* **Tickets:** pick **Write ticket** to hand a paper fine. The pad lists the offences your rank may fine, priced in minutes of a civilian's work: minor road $50 to $75 (speeding up to 15 over, parking, inspection),
  road $150 to $250 (speeding 16 to 30 over, reckless driving, an overloaded vehicle), serious road $300 to $400 (speeding more than 30 over, a crash, driving without a license),
  weapons $250 to $800. Robbery, murder and the other violent charges are jail only, 10 to 15 minutes. Up to three unpaid tickets per person.
* **Spike strips and roadblocks** are in the kit.
* **Hands up:** a suspect presses **F7** (gamepad: right bumper + X). That counts as complying, so you can search and cuff them.

## Jail, fines and the wanted board

* The **cell** is inside the station. A sentence is **60 seconds per wanted level**, at most **15 minutes**. When it ends the wanted level drops by one.
  Time **in jail counts online time**: it pauses when you log out and survives a restart. A prisoner sees **"Jail: m:ss left"** at the top of their screen, and the cell door opens when it reaches zero.
* A prisoner can **work off time**: the **laundry tub**, the **kitchen sink** and the **yard broom** each take 24 to 30 seconds off after a short job (scaled to the 15 minute cap), with a 30 second wait between repeats of the same chore.
* **Bail:** a prisoner can use the **bail phone** in the cell to pay bail from their **bank** and walk out at once. Bail is **$150 per minute** left, at least **$500**. Not enough in the bank, no bail; cash does not count.
* A **ticket** is paid at the **punch clock of the station duty desk**, in **cash**, oldest first. After **30 minutes** unpaid it doubles. After another 30 minutes the person gets **+1 wanted** and a poster on the wanted board.
* The **wanted board** and the **case board** inside the station list who is wanted and why. The **dispatch board** shows reports from payphones.

## Calls, evidence and the armoury

* Players call you from a **payphone**: *Report a crime*, *Report an emergency* or *Report someone suspicious*. An on-duty officer hears it on the radio and sees it on the board.
* On the radio, a **dispatcher** reads each call to you after a short squelch: what happened, then the place ("Robbery in progress. At the general store."). The written line follows on screen.
  A caller can ring once a minute; with nobody on duty nothing connects.
* The **armoury** hands out kit by rank. The **evidence locker** is in the station.
* A fired **alarm** at the bank, a shop till or the cash van also goes to dispatch.

The station, the cars and the gun store are on the [Saint-Philippe map](maps.md) and the [crime map](maps.md#crime-and-police-saint-philippe).
