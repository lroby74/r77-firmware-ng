# RetroN 77 HD firmware

A firmware image for the Hyperkin RetroN 77 built around **Stella 7.0**, with a
set of console-side additions that are not available anywhere else: a live
overclock/underclock menu, an on-screen SoC temperature readout, a self-expanding
SD card image, and three real emulation bugs fixed.

As far as we can tell this is the **first build of Stella 7.0 that runs on a
RetroN 77**. Stella ships a RetroN 77 target, but it had evidently not been
compiled for some time: one of its own source files no longer matched the class
it implements, so the 7.0 tree does not build for this console until that is
corrected. The one-line fix is in this fork and is described below.

This is a fork of [DirtyHairy's `r77-firmware-ng`](https://github.com/DirtyHairy/r77-firmware-ng),
which was based on Stella 6.6. Everything listed under
[What is different](#what-is-different) is new or changed relative to that firmware.

> **This fork is not affiliated with the Stella project and is not intended to be
> merged upstream.** Please do not report problems with this image to the Stella
> team — the changes here are specific to the RetroN 77 hardware.

---

## Quick start

**1. Write the `.img` file to a microSD card.**

On Windows, use **Win32 Disk Imager**. balenaEtcher reports a **false positive**
error on this image under Windows — the card it writes is perfectly good, but the
error message makes it look as if the write had failed. On Linux and macOS, `dd`
is fine.

**2. Put the card in the console and switch it on.**

> ⚠️ **The screen stays black for about 30 seconds before anything appears.**
> This is normal — the console is booting and getting ready. After that a
> full-screen **DO NOT TURN OFF THE CONSOLE** message appears with a live file
> counter while the card is expanded to its full size (FAT32, up to 32 GB).
> The whole thing takes a minute or two.
>
> **Do not switch the console off during any of this**, black screen included.
> The card is being reformatted; interrupting it leaves it unusable and you will
> have to write the image again.

### Which microSD card to use

**1 GB minimum, 4-32 GB recommended, from a brand you recognise.** Below 1 GB is
not supported: the self-expanding step reformats the card and, under 512 MiB,
produces a filesystem that does not match what this image declares. The tiny
card supplied with the console is one of those — it boots once, then black
screen.

A worn card also causes symptoms that look exactly like firmware bugs. If you
see behaviour nobody else reports, try another card first.

**3. Copy your games.**

Switch off, put the card back in your PC, and copy your ROMs into the `games`
folder. From now on the console starts normally — the expansion happens once and
never again.

**If you already run this firmware**, you do not need to reflash: the whole
system lives inside `uImage`, so replacing that single file on the card updates
everything — your games and settings stay where they are.

---

## What is different

### Emulator

Built on **Stella 7.0**. The previous release of this firmware shipped 6.7.1;
DirtyHairy's shipped 6.6.

Stella 7.0's changelog claims *"accelerated emulation up to ~15% (ARM)"*. On this
console the effect is visible without needing a benchmark: same console, same
games, same clock settings, and it runs **measurably cooler** than under 6.7.1.
A 1200 MHz setting that used to lock up within a couple of minutes now survives
around twenty. It still locks up eventually, so the recommendation below has not
changed — but the machine is plainly doing less work for the same result.

The upstream gains are in the places that matter here: the ARM interpreter that
every Champ Games, DPC+, CDF and CDFJ cartridge runs on, and the per-pixel TIA
sprite code and audio channel generator, which are busy in *every* title.

**One upstream fix was needed to get this far.** `OSystemR77.cxx` still declared
`getBaseDirectories()` with the old parameter type, so it no longer overrode the
base class method and the compiler rejected the file outright. One line, and
without it the RetroN 77 target of Stella 7.0 does not compile at all.

### Emulation bugs fixed

Three defects that affect what you actually see on screen. All three were
verified by comparing detection results across a large ROM set before and after
the change.

> The 128 KB Turbo Arcade detection bug, fixed by hand in the 6.7.1 release of
> this firmware, is **fixed upstream in 7.0** and no longer needs a patch here.

* **QuadTari + SaveKey are no longer mutually exclusive.**
  Controller auto-detection tested for a SaveKey *before* testing for a QuadTari,
  so in games that support both, the QuadTari lost and two-player mode silently
  disappeared. The order is now reversed, and when a ROM asks for a QuadTari and
  also contains SaveKey code, the right port is configured as
  `Joystick + SaveKey`. Measured across 78 ROMs: 17 changed behaviour,
  61 were untouched.

* **A joystick is no longer mistaken for a paddle in QuadTari games.**
  New in Stella 7.0: the emulator now works out what is plugged *into* the
  QuadTari, and when no signature matches it assumes paddles. Champ Games
  titles read the fire button from the cartridge's own ARM code rather than
  from the 6507, so that detection never matches and they all ended up as
  paddles — the joystick simply did not steer. The assumption is now skipped
  when the ROM reads the joystick directions. Measured across 925 ROMs: 22 use
  a QuadTari, **5 were wrong and are now right, none changed for the worse**
  (Galagon, rubyQ, Wizard of Wor Arcade, Turbo Arcade demo).

  This one is a defect in Stella 7.0 itself rather than in this firmware.
  Stella 6.7.1 is not affected — it never detected the
  QuadTari's sub-controllers at all.

* **Developer mode can no longer break every ARM game.**
  Turning on Stella's *Developer* settings enabled a fatal trap in the ARM
  interpreter plus per-frame state saving. On this hardware that combination
  stops **every** ARM-based title (all of the Champ Games catalogue, DPC+, CDF,
  CDFJ) from starting at all. Developer mode is now forced off on the RetroN 77.

### New: "OC settings" menu

Reachable from the options menu (it is present in both the basic and the
advanced settings screens).

| Setting | Values | Applied |
|---|---|---|
| **CPU** | 1200 (OC) · 1104 · 1008 · 912 (UC) | immediately |
| **CPU in menus** | 480 · 600 · 648 MHz · same as game | immediately |
| **GPU** | 168 (UC) · 252 (std) · 312 · 384 · 456 | immediately |
| **DRAM** | 408 · 480 · 504 · 624 (std) · 648 · 672 · 696 · 720 (OC) | on reboot |

These are the real steps in the kernel's frequency table, not arbitrary numbers.

* **CPU in menus** drops the clock in the launcher and in the menus, where there
  is nothing to emulate, and restores your chosen speed the moment a game
  starts. It never goes *above* your game setting. This keeps the console
  noticeably cooler while it sits idle.
* **The GPU drops with it.** While the same setting is active, the GPU also
  falls to its lowest step, 168 MHz, and returns to your chosen frequency when
  a game starts. Menus and the launcher have nothing to draw that needs more.
  Like the CPU, it never goes above what you selected.
* **The GPU knob did not exist before.** The compiled Mali driver hard-codes
  252 MHz and exposes no control at all; this firmware adds a `sysfs` entry to
  the driver so the frequency can be changed at runtime.
* **The DRAM clock is set by U-Boot, not by Linux.** Changing it rewrites the
  U-Boot SPL on the card (at the 8 KiB offset, with read-back verification) and
  takes effect on the next boot. All eight variants ship inside the image.
* A live status line shows the actual CPU and GPU frequency plus the SoC
  temperature, refreshed about three times a second.

> **Above 624 MHz the DRAM voltage is not raised.** An unstable memory clock does
> not make things slow, it corrupts memory silently. Test it with the built-in
> memory tester (create a file called `memtest` in the root of the card), not by
> playing.

### What to actually set

**What you should set depends entirely on how well your console gets rid of
heat.** The figures below were measured on a real console using the on-screen
temperature readout, while playing, not idling.

| Cooling | Peak temperature at 1104 MHz | Set the CPU to |
|---|---|---|
| Stock aluminium heatsink, closed case | hits **75 °C** and throttles down | **912 MHz** |
| Copper heatsinks on SoC *and* DRAM, closed case | still hits **75 °C** and throttles down | **912 MHz** |
| Copper heatsinks + a fan under the console | **70 °C**, steady | **1104 MHz** |
| Copper + fan, **lid removed** | **62 °C**, with GPU *and* DRAM also at maximum | **1104 MHz**, everything maxed |

The result worth understanding: **the limit is not the chip, it is the heat
trapped inside the closed box.** Better heatsinks alone did not stop the
throttling — the copper still reached 75 °C, because the warm air had nowhere to
go. Adding a fan fixed it. Removing the lid, so the fan reaches the whole board,
brought it down to 62 °C *while simultaneously* running the GPU at maximum and
the DRAM at roughly 720 MHz — the settings that had been assumed to be the risky
part, and were not.

So: stock and closed, **912 MHz**. With added airflow, **1104 MHz**, and stop
worrying about it.

| Other settings | Recommended |
|---|---|
| **CPU in menus** | 480 MHz |
| **GPU** | 252 MHz (the boot value) is fine for everything; raise it only if you have airflow |
| **DRAM** | **624 MHz** unless you have validated higher — see the DRAM note below |

**Why 912 MHz if you cannot improve the cooling.** It runs at a lower core
voltage, so it takes much longer to reach 75 °C — and 75 °C is precisely the
temperature at which the thermal governor *forces* the CPU down to 912 MHz
anyway (see [Thermal throttling](#thermal-throttling)). Selecting 1104 MHz on a
console that gets hot does not give you 1104 MHz; it gives you 912 MHz, reached
by producing more heat on the way there. Choosing 912 deliberately gives you a
clock that never changes under your feet.

**Why the recommendation skips 1008 MHz.** Because of where the core voltage
step actually is. From the board's own DVFS table:

| Step | Core voltage |
|---|---|
| 1200 MHz | 1.30 V |
| 1104 MHz | 1.30 V |
| **1008 MHz** | **1.30 V** |
| **912 MHz** | **1.10 V** |

The voltage drop is between 912 and 1008, not higher up — so 1008 MHz already
pays the full voltage, exactly like 1104. Since heat goes with voltage squared
times frequency, taking 912 MHz as the reference:

* **912 MHz at 1.10 V** — the reference
* **1008 MHz at 1.30 V** — roughly **+54%** power
* **1104 MHz at 1.30 V** — roughly **+69%** power

So 1008 MHz is an awkward middle ground: nearly all the heat of 1104 for 9% less
speed. If you cannot cool the console, the step that keeps you away from 75 °C
is 912, because that is where the voltage falls. If you *can* cool it, there is
no reason to stop below 1104.

### The one title that exposes 912 MHz

**Battlezone.** At 912 MHz its sound breaks up audibly; at 1008 MHz and above it
is clean. This is neither an emulator bug nor something you can tune around, and
the reason is arithmetic: the built-in benchmark puts Battlezone at a **60.2 fps
ceiling** at 912 MHz, and that figure is *pure emulation*, before a single pixel
is drawn. Add the cost of drawing the frame and the console falls below 60 fps,
so the audio buffer runs dry over and over.

Two consequences, both tested rather than assumed:

* **Raising `Headroom` and `Buffer size` in the Audio menu does not help.** A
  larger reserve absorbs the occasional late frame; it cannot fill a shortfall
  that returns every frame. Tried; no effect.
* **The only cure is more CPU** — a single step up to 1008 MHz is enough.

If you play the demanding classics and cannot add cooling, this is the reason to
pick 1008 MHz despite the voltage argument above.

**Avoid 1200 MHz — this is the firmest recommendation in this document.** It is
the highest entry in the board's own frequency table, with no margin left above
it, and consoles lock up there.

Stella 7.0 made this test much more interesting, because it runs cool enough
that 1200 MHz becomes *usable* for a while. It was measured again on a console
modified about as far as this one can sensibly be modified — copper heatsinks
replacing every aluminium one, a fan strapped underneath, and the lid left off
permanently so the fan reaches the whole board:

| Conditions | Result |
|---|---|
| Fan off, temperature climbing slowly | reached **70 °C**, then froze |
| Fan on, **58 °C rock steady** for 23 minutes | froze anyway |

The second line is the one that settles it. **The console froze at a
temperature it had been holding without effort**, on the most heavily cooled
setup available. That rules out heat: 1200 MHz is simply beyond what this
silicon does reliably at the voltage available, and no amount of cooling will
change it. The previous firmware shipped 1.2 GHz as its default; this one does
not, for exactly that reason.

If you want the extra speed, the way to get it is airflow at 1104 MHz, not the
next step up.

**The DRAM options above 624 MHz are a test instrument, not a performance
setting.** They exist so you can find out how good the memory chips soldered to
*your* board happen to be — that varies from unit to unit. Use them with the
memory tester, confirm what your board tolerates, and then go back to 624 for
playing. There is nothing to gain in a 2600 emulator from a faster memory clock,
and a great deal to lose from a marginally unstable one.

### New: SoC temperature on screen while playing

A single line at the bottom of the screen showing temperature and the current CPU
and GPU clocks, updated once a second:

```
54 C   CPU 1104 MHz   GPU 384 MHz
```

* Toggle it from the Commands menu (rear **"4:3 / 16:9"** button) with the
  **Temp On / Temp Off** entry.
* The setting is remembered across reboots.

**Optional shortcut on the rear COLOR / B-W button.** In *OC settings* there is a
**B/W button** entry that can reassign that rear button to switch the temperature
on and off with a single press. **It is off by default, deliberately.** The
colour / black-and-white switch is not decoration — a good number of games use it
as a real game control:

| Game | What the B/W switch does |
|---|---|
| Starmaster | opens the Galactic Chart |
| Space Shuttle | fires the main thrusters |
| Secret Quest | recalls the player status screen and enters the password |
| Cosmic Ark | turns the starfield on and off |
| Solaris | inverts the planet horizons |
| Ghost Manor | chooses the character's sex |
| Bump 'n' Jump | turns the background music on and off |
| Riddle of the Sphinx | toggles an in-game function |

Turn the shortcut on only if you do not play any of those; the switch itself
always remains available from the Commands menu, third entry in the first
column, below Select and Reset.

### New: self-expanding SD card image

The image is 48 MB and grows to the size of your card on first boot.

* FAT32, capped at 32 GB.
* A full-screen **"DO NOT TURN OFF THE CONSOLE"** message with a live
  `n / total` file counter, so nobody pulls the plug on what looks like a hang.
  (This needed a small framebuffer renderer: the kernel is built without a
  framebuffer console, so ordinary text output is invisible.)
* The message appears after roughly **30 seconds of black screen** — that part is
  the console booting, before the emulator or any of this code runs, and there is
  nothing to display during it. It is not a hang.
* It only runs when the `expand` marker file is present, and the file is deleted
  once it succeeds — replacing `uImage` on a card you already use will never
  reformat anything.
* The image ships with a correct BPB *hidden sectors* field. `mformat` leaves it
  at zero, which makes Windows treat the volume as needing repair and write to it
  the moment it is mounted — which in turn makes the *verify* pass of Windows
  imaging tools fail even though the write was perfectly fine.

### Changed defaults

* **The image boots at 1104 MHz, not 1200 MHz**, because a number of units lock
  up at 1.2 GHz. See [What to actually set](#what-to-actually-set). If you really
  want the old behaviour, edit `sys/settings` on the card and comment the
  `CPU_FREQ` line out.
* **In the menus the CPU drops to 480 MHz and the GPU to 168 MHz**, and both
  return to your settings the moment a game starts.
* GPU stays at its 252 MHz boot value and DRAM at 624 MHz until you change them.

### Extras for tinkerers

* **`BENCH=600` in `sys/settings`** — at the start of every ROM the console
  emulates that many frames flat out and reports the achieved frames per second
  on screen. Useful for comparing clock settings or builds on the real hardware
  instead of guessing. Set it back to 0 when you are done.
* **`sys/stella-test`** — if this file exists it replaces the emulator binary at
  boot, so alternative builds can be tried by copying one file instead of
  rebuilding the kernel. Remove it to go back.
* **`/sys/kernel/atarikey/raw`** — a read-only kernel entry showing exactly what
  the controller microcontroller is sending right now. The OC settings screen
  displays it live.

---

## Console buttons

| Button | In a game | In the menus |
|---|---|---|
| **MODE** (front) | Select | next tab |
| **RESET** (front) | Reset | down |
| **SKILL P1** (front) | left difficulty | confirm |
| **SKILL P2** (front) | right difficulty | cancel |
| **SAVE** (front) | save state | up |
| **LOAD** (front) | load state | previous tab |
| **COLOR / B-W** (rear) | colour / B&W switch — *optionally* temperature on/off | — |
| **4:3 / 16:9** (rear) | Commands menu | — |
| **FRY** (rear) | exit to the launcher | — |

---

## `sys/settings`

A plain text file on the card. Lines that this firmware does not know are left
alone, so it is safe to keep your own entries in it.

```
CPU_FREQ=1104000     # 1200000 / 1104000 / 1008000 / 912000
CPU_IDLE=480000      # clock used in the launcher and menus, 0 = same as game
GPU_FREQ=252         # 168 / 252 / 312 / 384 / 456
DRAM_CLK=624         # record of what was written to U-Boot; change from the menu
DUMP_TO_SD=1         # save cartridge dumps to the card
BENCH=0              # benchmark frames at ROM start, 0 = off
```

`DONT_OVERCLOCK` from the older firmware is still honoured, and means the same as
`CPU_FREQ=1008000`.

---

## Thermal throttling

The console protects itself by lowering the CPU clock as it heats up. The trip
points are not a Linux setting — they are baked into the board configuration that
U-Boot hands to the kernel, and they are the same on the original firmware:

| Temperature | What happens |
|---|---|
| **75 °C** | first step — the CPU is held at **912 MHz**, whatever you selected |
| **80 °C** | second step |
| **85 °C** | third step |
| **90 °C** | fourth step |
| **95 °C** | fifth step |
| **105 °C** | last step, and the driver's critical point — the console shuts down |

Each step past the first reduces the clock further. The important one is the
**first**: from 75 °C upwards your CPU setting stops meaning anything, because
the thermal governor is choosing instead of you. Setting 1104 or 1200 MHz on a
console that sits at 78 °C achieves nothing at all except more heat.

This is also why **912 MHz is the sensible everyday setting**: it is the speed
the console falls back to anyway when it gets warm, so choosing it deliberately
gives you a clock that never changes under your feet, and a machine that stays
well clear of the first trip point.

**What the readout shows.** The temperature sensor reports six channels; the
on-screen value and the one in the OC settings screen are the **hottest** of the
six, which is the number that matters for throttling. For reference: with the
stock passive heatsink and 1104 MHz / GPU 456 / DRAM 624, normal play has been
measured at **63–70 °C** — comfortable, but only about five degrees from the
first step.

---

## Cooling — where the real gain is

The RetroN 77 is cooled entirely passively inside a closed plastic case, and
that case is the actual limit. Measured on a real console at 1104 MHz, playing:

| What was fitted | Peak temperature |
|---|---|
| Stock aluminium heatsink | **75 °C** — throttles to 912 MHz |
| Copper heatsinks on SoC *and* DRAM | **75 °C** — still throttles |
| Copper + a fan under the console | **70 °C** |
| Copper + fan, **lid removed** | **62 °C** — with GPU and DRAM at maximum too |

Read that table carefully, because it is not the result most people expect:
**better heatsinks on their own did not stop the throttling.** Copper on both
the SoC and the DRAM still reached 75 °C, because the heat had nowhere to go
once it left the metal. What actually changed the outcome was **moving air**.

In order of what your effort buys you:

1. **Airflow.** A small fan under the console is worth more than any heatsink
   upgrade — it was the difference between throttling and not throttling. With
   the lid off so the air reaches the whole board, the console held 62 °C while
   running the CPU at 1104 MHz, the GPU at maximum and the DRAM at roughly
   720 MHz, all at once.
2. **A copper SoC heatsink** in place of the stock aluminium one. A drop-in
   change, and it helps — but on its own, in a closed case, it did not move the
   console off the 75 °C trip point.
3. **A small copper heatsink on the DRAM**, which has none at all from the
   factory. Cheap, and it matters if you intend to raise the memory clock.

Use the on-screen temperature readout to judge your own changes: switch it on
from the Commands menu, play for a quarter of an hour, and note the **highest**
figure you see, not the one at the end. Every degree you remove is a degree of
distance from the 75 °C step described above — and below that line the console
runs at the speed you selected, instead of the 912 MHz that thermal throttling
imposes.

---

## Known limitations

* **The console's own controller ports cannot provide a second fire button.**
  This was investigated on real hardware by reading the raw microcontroller
  output: the second button is simply never transmitted, on either an Atari 7800
  pad or a Mega Drive pad. A menu entry for it was built and then removed,
  because it could not work. **USB gamepads do have full button support**, so
  Booster Grip and Genesis controllers work when you use one.
* **Paddles and a second button share the same physical line.** Enabling one
  would disable the other; see above.
* **Cartridge dumping is slow by design.** The cartridge is not on a bus — its
  contents arrive through a microcontroller at 115200 baud, roughly one byte
  every 87 µs against the 838 ns of a real 2600. Nothing in software can change
  this.
* **From 75 °C the console throttles itself.** It drops the CPU to 912 MHz on
  its own, whatever you selected in the menu — see
  [Thermal throttling](#thermal-throttling).
* USB devices (2600-daptor, USB-to-serial dongles) may still cause slowdowns in
  ARM-based games, as in the previous firmware.

---

## Disclaimer

**You use this firmware at your own risk.** None of the developers are liable for
any damage to your console, your SD cards, or your peace of mind. The overclock
and underclock options change clocks outside the manufacturer's configuration;
the DRAM options in particular can corrupt data if pushed beyond what your
particular board tolerates. Test them properly before trusting them with save
files.

---

## Credits

* **Stella** — the [Stella team](https://stella-emu.github.io/). This firmware
  merely packages it.
* **`r77-firmware-ng`** — [DirtyHairy](https://github.com/DirtyHairy/r77-firmware-ng),
  the Stella 6 firmware this fork is built on.
* **Remowilliams**, for the original AtariAge community build.
* **Hyperkin**, for publishing the original source drop.

Licensing follows the upstream projects; Stella is GPL v2.

---

## Building

The upstream build instructions still apply, but **building Stella 7.0 for this
console needs five things that nothing tells you about**, plus the compiler
being newer than the one this firmware normally uses. They are listed first,
because without them the build either fails or produces a binary that does not
start on the console.

**1. GCC 13 or newer is required.** Stella 7.0 uses `using enum`, a C++20
feature the shipped GCC 10 does not implement, and `ostringstream::view()`,
which needs libstdc++ 11. The Arm GNU Toolchain 13.3 (`arm-none-linux-gnueabihf`)
works. Everything below is a consequence of using a newer compiler against the
console's much older system libraries.

**2. Build against the old sysroot, not the new toolchain's.** The console runs
glibc 2.33. Pass `--sysroot` pointing at the *original* toolchain's libc, or the
binary will ask for symbols the console does not have and die at startup with no
message.

**3. Disable the new toolchain's fixed `pthread.h`.** GCC 13 ships a "corrected"
copy built for glibc 2.38, which includes a header that does not exist in 2.33.
Rename `lib/gcc/arm-none-linux-gnueabihf/13.3.1/include-fixed/pthread.h` out of
the way; the sysroot's own header is the right one.

**4. Link libstdc++ statically.** GCC 13's C++ library needs symbol versions the
console's libstdc++ does not provide. Add `-static-libstdc++ -static-libgcc`
**to `LDFLAGS`**. Note that 7.0's `config.mak` writes `LDFLAGS +=`, not
`LDFLAGS :=` — a substitution written for the older file silently does nothing,
and the failure only shows up as a pile of undefined `GLIBC_2.34` symbols much
later.

**5. Two glibc functions have to be supplied by hand.** GCC 13's headers route
`strtoul` to `__isoc23_strtoul`, and 7.0 calls `arc4random`; neither exists in
glibc 2.33. A dozen lines forwarding them to `strtoul` and `getrandom` are
enough, linked in through `LIBS`.

**Verify the result before copying it to a card.** Two checks, both of which
must pass, or the console shows a black screen and tells you nothing:

```
readelf -d  stella | grep libstdc++          # must print nothing
readelf -sW stella | grep -oE 'GLIBC_2\.[0-9]+' | sort -uV | tail -1   # must not exceed 2.33
```

Three further things are worth writing down.

**Do *not* build with `make RELEASE=1`.** That option enables profile-guided
optimisation, and on this hardware **it makes Stella slower**. Measured on the
console with the built-in benchmark, at 912 MHz, sound off, non-PGO against PGO
from identical sources:

| Game | with PGO | without PGO |
|---|---|---|
| Kaboom! | 85.3 fps | **94.4 fps** |
| Ms Pac-Man | 65.7 fps | **70.4 fps** |
| Battlezone | 56.4 fps | **60.2 fps** |
| Elevator Agent | 43.3 fps | **45.6 fps** |

Four titles out of four, 5–11% in favour of the plain build. Two likely reasons:
the profile is collected by running the ARM binary under `qemu-arm`, which
behaves nothing like a Cortex-A7; and the profiling run only exercises two ROMs,
leaving **225 source files with no profile data at all** — which `-fprofile-use`
then treats as cold. The PGO binary is also 16% larger, which a 32 KB L1
instruction cache does not appreciate.

Those figures were measured on the 6.7.1 build; nothing about 7.0 changes the
reasoning. The released image is therefore built with a plain `make`. If you want to try
PGO anyway, note that the profiling step needs a working `qemu-user`: under
WSL 1 it cannot map the guest address space at all and the build dies there;
WSL 2, ordinary Linux, or the upstream Docker container are fine.

**Stella still leaves `src/sqlite` out of its core module list**, so it has to be
added back in `app/stella/config/config.mak` or the link fails on about thirty
`sqlite3_*` symbols. Unchanged since 6.7.1.

**If you change the architecture flags, change them in `LDFLAGS` too.** With
`-flto` most code generation happens at link time, so mixing `-mcpu=cortex-a7` in
`CXXFLAGS` with the `-march=armv7-a` left in `LDFLAGS` makes gcc 10 abort with an
internal compiler error. Note that `-mthumb` is a no-op here: the Arm toolchain
is configured `--with-mode=thumb`, so this firmware has always been Thumb-2.
