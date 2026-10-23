# Notes to publish later (temporary)

Collected while building and testing. Nothing here is on the website yet; pick from it for the FAQ
and the docs when Phosphor is released.

## FAQ: controllers (how MiSTer works, with or without Phosphor)

None of these are limits of Phosphor: it's how MiSTer itself works, and Phosphor leaves it that way.

- **Two controllers for a two-player game: which one is player 1?** The one that connected first.
  MiSTer numbers controllers in the order they connect, so switch on (or plug in) player 1's
  controller first. MiSTer.ini's `player_1_controller`, `player_2_controller`... pin a controller to
  a player; they aren't in MiSTer's menu.
- **Two of the same controller: set up the buttons twice?** No. MiSTer keeps one mapping per
  controller model (`config/inputs/input_<vid>_<pid>_v3.map`), so mapping one sets up all of that
  model. MiSTer.ini's `controller_unique_mapping` gives identical controllers their own mappings.
- **Does a mapping work in games too?** Yes. The menu's mapping is used by the menu and every core.
  A core can have its own (Define joystick buttons in that core's menu), which wins for that core.
- Checked in MiSTer's source (input.cpp, cfg.cpp), not yet tried with two controllers on an SS1.

## Docs: power

- The SS1 needs a solid supply. Use a single-port USB-C PD adapter, 9 V / 3 A or more. Multi-port
  chargers share their power and can drop the voltage when something else is plugged in.
- A brownout leaves no trace (no crash, no error, no log): the SS1 just restarts. Phosphor's power
  check counts heavy work that never finished; after 3, the status bar says "Power supply problem?".
- NVMe drives rated above about 7 W can be too much for the dock (Phosphor warns in Storage).

## Things we learned

- **Controllers:** MiSTer's Main sends Phosphor the pad's buttons through its menu mapping, so on the
  SS1's 8BitDo pads X and Y arrive as "select" and "start". Phosphor reads Select, Start, X and Y
  from the controller itself.
- **The dock's NVMe bridge (JMicron JMS583, 152d:0583)** answers the NVMe health log (temperature,
  wear, hours, unsafe shutdowns) but not Identify: it times out and USB resets the dock with the
  drive mounted. So the drive's model can't be read through the dock, and nothing should ask.
  SS1 Tool's helper asks too, and has the same problem.
- **"Windows says the card needs fixing" after a reflash:** exFAT's dirty flag. Linux keeps a flag that
  was already set at mount until the drive is checked; it was the NVMe in our case. Safe shut down
  (Select + Start) leaves the drives clean; a drive already dirty needs a check (Storage > Repair).
- **Update All and the static between it and Phosphor:** the update replaces menu.rbf; Main loads
  Phosphor's menu core (with its loading screen) wherever it would load menu.rbf.
- **Main reads MiSTer.ini only when it starts**: changes need a restart.
