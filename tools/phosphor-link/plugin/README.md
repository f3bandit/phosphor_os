# Phosphor Link 0.4.2

Lets Claude run commands, copy files, watch the console and press buttons on a SuperStation One.

- SSH first when the SS1 is on the network (the address comes from the Phosphor app's pairing),
  the dock's USB-C debug port otherwise. The boot log and reboots always use the debug port.
- Watchdog: SSH keepalives, Ctrl-C and a check when the console stops answering, the link reopened
  by itself, and an old copy still holding the COM port stopped.
- Logs in with MiSTer's own SSH login (Phosphor 0.27.38 and later have no SSH settings of their own;
  a key from an older Phosphor app is still tried first when there is one).
- The program is inside the plugin, so updating is just installing the next package. Settings
  (optional) go in phosphor-link.ini next to server/phosphor-link.exe.

## Install (Windows)

In Claude Code, add the repo as a plugin marketplace and install it:

    /plugin marketplace add f3bandit/phosphor_os
    /plugin install phosphor-link@phosphor

The repository is private: Claude Code uses your GitHub login (the git or gh credentials on this PC)
to read it. `/plugin marketplace update phosphor` picks up a new version.
