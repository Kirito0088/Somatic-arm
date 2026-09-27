# Somatic Arm: tooling, skills/connectors, and whether ML is needed

Date: 2026-09-27. Scope: firmware `firmware/somatic_arm/somatic_arm.ino` (Uno R3, 1-channel EMG on A0, 3x MG90S, binary open/close).
Legend: **[verified]** = checked locally on this machine or in primary source; **[unverified]** = not confirmed from a primary source in this pass.

## TL;DR

1. **The registered MCP server is broken, and it can't upload anyway.** `arduino-claude-mcp`'s `bin` starts a REST server, not an MCP stdio server. Its only build tool is compile. It has no upload, board-list, or serial tool. Drop it and have agents call **arduino-cli** directly through Bash.
2. **arduino-cli is already on disk**, bundled with Arduino IDE 2 (v1.5.1). With the IDE's config file it compiles this sketch today: 5708 B flash (17%), 287 B RAM (14%). For PATH use, `winget install --id ArduinoSA.CLI -e` is a real Arduino SA package.
3. **Logging:** use a ~20-line pyserial script, `emg:… close:… open:…` → CSV. For quick viewing, use `arduino-cli monitor --timestamp`. No serial MCP is needed.
4. **Testing without hardware:** move the threshold, hysteresis, and max-hold logic into a pure-C++ header. Test it with the `g++` 15.2 already installed (or PlatformIO `native` + Unity). Use Wokwi (Uno + servo + potentiometer on A0) for an optional end-to-end demo.
5. **ML: not needed and not useful at this scope.** The sensor outputs a rectified envelope, which is what commercial direct control uses. Pattern recognition needs raw EMG, usually several channels, and a 32-bit board. Upgrade path is in §5.

---

## 1. Build/flash toolchain for agents

**What `arduino-claude-mcp` actually is** [verified: installed package v0.1.2 source + [README](https://github.com/eoinjordan/arduino-mcp)]
- There are two entry points. `dist/index.js` is an Express REST server on port 3080 (`app.listen`). `build/mcp.js` is the MCP stdio bridge, and it forwards every tool call over HTTP to that REST server.
- `package.json` `"bin": {"arduino-claude-mcp": "./dist/index.js"}` means the command in our `.mcp.json` (`"command": "arduino-claude-mcp"`) launches the **REST server**. That process never speaks MCP JSON-RPC on stdio. The README's own client config uses `node /absolute/path/to/build/mcp.js`, and that also needs the REST server running as a second process.
- The MCP tools are `validate`, `list_sources`, `read_source`, `write_source`, `append_source`, and `build`. `build` only runs `arduino-cli compile --fqbn $ARDUINO_FQBN <dir>`. It needs `arduino-cli` (or `ARDUINO_CLI`) and `ARDUINO_FQBN` set on the REST process. It does **not** need the Arduino IDE running, but it has **no upload, board list, or serial monitor**. `docs/agents/arduino-mcp.md` claims upload and board listing, and that is inaccurate.
- `mcp.js` also `console.log`s a banner to stdout after connecting. That can corrupt a stdio JSON-RPC stream [unverified whether Claude Code tolerates it].
- Upstream: one npm version (0.1.2, 2026-01-31), 0 stars, last push 2026-02-23 (`gh api`).

**Official arduino-cli** [verified]
- It is bundled with the IDE at `C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe` (v1.5.1). `arduino:avr` 1.8.8 is already installed in `%LOCALAPPDATA%\Arduino15`.
- The sketch compiles **only if** the IDE's built-in library dir is visible. Without it you get `fatal error: Servo.h: No such file or directory`. Two fixes:
  - pass `--config-file %USERPROFILE%\.arduinoIDE\arduino-cli.yaml`, which sets `directories.builtin.libraries`, or
  - run `arduino-cli lib install Servo` once.
- Core commands: `board list`, `compile --fqbn`, `upload -p -b`, `lib install` ([getting started](https://arduino.github.io/arduino-cli/1.5/getting-started/)). `compile` also has `-u/--upload`, `-p`, and `--output-dir` (from `compile --help`). `monitor -p <port> --config 115200` with `--quiet`, `--raw`, and `--timestamp` ([monitor ref](https://arduino.github.io/arduino-cli/1.5/commands/arduino-cli_monitor/)).
- Windows install per the [official page](https://arduino.github.io/arduino-cli/1.5/installation/): zip, MSI, or the install script. That page doesn't mention winget. However, `winget show --id ArduinoSA.CLI` resolves to publisher **Arduino SA**, v1.5.1, homepage github.com/arduino/arduino-cli [verified locally].

**Other Arduino MCP servers**
- None of them is from Arduino. The MCP Registry lists [`hardware-mcp/arduino-mcp-server`](https://github.com/hardware-mcp/arduino-mcp-server) (npm `arduino-mcp-server`, 0.2.8, MIT, ~19 stars). It wraps arduino-cli and offers board detection, compile/upload, stateful serial sessions, and "safety preflight". It needs arduino-cli on PATH and Node 20+ ([registry query](https://registry.modelcontextprotocol.io/v0/servers?search=arduino)) [not tested].
- There is also PyPI `mcp-arduino-server` (FastMCP, Python ≥3.10, arduino-cli on PATH) [README-level only, unverified].
- No Arduino, embedded, or serial connector appears in the Anthropic connector registry search.

**Recommendation (minimal, reliable):** don't use an MCP for this.
- Put `arduino-cli` on PATH (winget) or point at the bundled exe.
- Have agents run `compile`, `upload`, and `monitor` via Bash. Upload and monitor need the board plugged in and a human present.
- Remove the `eoinjordan-arduino-mcp` entry from `.mcp.json` and correct `docs/agents/arduino-mcp.md`. If you want MCP tools later, trial `hardware-mcp/arduino-mcp-server`.

## 2. Serial capture / logging to CSV

- **Quick look:** `arduino-cli monitor -p COMx --config 115200 --quiet --timestamp`. It is interactive and runs until killed, so an agent must run it in the background with a timeout.
- **Logging:** pyserial. Install with `python -m pip install pyserial` ([docs](https://pyserial.readthedocs.io/en/latest/pyserial.html)). pyserial is **not** installed here yet [verified]. Always set a `timeout`, because `readline()` "could block forever" without one ([short intro](https://pyserial.readthedocs.io/en/latest/shortintro.html)). Find the port with `python -m serial.tools.list_ports` ([tools](https://pyserial.readthedocs.io/en/latest/tools.html)).
- At 50 lines/s × ~30 B, the stream is ~1.5 kB/s, well under 115200 baud.

Minimal logger (save as e.g. `tools/log_emg.py`; parses any `key:value` pairs):
```python
import csv, sys, time, serial
port, secs, out = sys.argv[1], float(sys.argv[2]), sys.argv[3]   # COM5 60 session.csv
with serial.Serial(port, 115200, timeout=1) as s, open(out, "w", newline="") as f:
    w, t0 = None, time.time()
    while time.time() - t0 < secs:
        line = s.readline().decode(errors="ignore").strip()
        try: row = dict(kv.split(":", 1) for kv in line.split())
        except ValueError: continue            # skip partial/garbage lines
        row = {"t_ms": int((time.time() - t0) * 1000), **row}
        if w is None: w = csv.DictWriter(f, fieldnames=list(row)); w.writeheader()
        if list(row)[1:] == w.fieldnames[1:]: w.writerow(row)
```

Caveats:
- Only one program can hold the COM port at a time: Serial Plotter, monitor, the logger, or `upload` [standard Windows behaviour; unverified cite].
- Opening the port normally **resets the Uno**. The ATmega16U2 DTR line is capacitor-coupled to RESET. So each logging run restarts `setup()` and re-runs the 6 s calibration, which is handy because it captures the calibration [documented on the legacy arduino.cc Uno page; could not re-fetch a primary copy, so treat as unverified].
- For tuning, add the firmware state (`closed`/`locked`) and raw `analogRead` to the print line. Right now only the smoothed level and thresholds are logged.

## 3. Testing firmware without hardware

- **Host unit tests (recommended).** Move the decision logic into a pure-C++ header in the sketch folder, e.g. `firmware/somatic_arm/emg_logic.h`. The logic is: EMA update, calibration maths (`closeAt`=midpoint, `openAt`=quarter, `MIN_SPAN`), hysteresis, max-hold → `locked` until below `openAt`. Write it as a `step(level, nowMs) → {OPEN, CLOSE, NONE}` function with no `millis()`, `analogRead`, or `Servo` calls inside. The Arduino IDE compiles `.h` files in the sketch folder. Then test with plain `g++`. MinGW-w64 **g++ 15.2 is already on PATH** [verified], so there is nothing to install.
- **PlatformIO `native` + Unity** is the structured alternative.
  - PlatformIO runs "the same tests on the local host machine (native)" ([unit testing](https://docs.platformio.org/en/latest/advanced/unit-testing/index.html)).
  - The native platform "does not install automatically any toolchains" and needs system GCC on PATH ([native](https://docs.platformio.org/en/latest/platforms/native.html)).
  - Unity is recommended for constrained targets ([frameworks](https://docs.platformio.org/en/latest/advanced/unit-testing/frameworks/index.html)). `pio` is not installed [verified].
- **ArduinoFake** ([repo](https://github.com/FabioBatSilva/ArduinoFake), FakeIt-based mocks of `digitalWrite`, `millis`, etc.) is only worth it if you *don't* extract the logic. It is heavier than needed here.
- **Wokwi can simulate this build.**
  - It has an Uno, a servo part with PWM/V+/GND pins, and examples including an Uno sweep and a potentiometer knob ([servo](https://docs.wokwi.com/parts/wokwi-servo)).
  - The potentiometer's SIG goes to an analog pin. Automation can set `position` 0.0–1.0 ([pot](https://docs.wokwi.com/parts/wokwi-potentiometer)), so a pot on A0 stands in for the EMG envelope.
  - Scenario YAML supports `set-control`, `delay`, `wait-serial`, and `expect-pin`, run with `wokwi-cli . --scenario x.yaml` ([scenarios](https://docs.wokwi.com/wokwi-ci/automation-scenarios)).
  - `wokwi.toml` points at the compiled `.hex`/`.elf` ([config](https://docs.wokwi.com/vscode/project-config)). `arduino-cli compile --output-dir` produces `somatic_arm.ino.hex`/`.elf` [verified].
  - The CLI needs `WOKWI_CLI_TOKEN`. The free tier is 50 sim-minutes/month ([CI](https://docs.wokwi.com/wokwi-ci/getting-started)). Windows install: `iwr https://wokwi.com/ci/install.ps1 -useb | iex` ([install](https://docs.wokwi.com/wokwi-ci/cli-installation)). The VS Code extension needs a free community licence ([VS Code](https://docs.wokwi.com/vscode/getting-started)).
  - Limits: the servo docs don't mention current, stall, or power modelling, so Wokwi won't catch brown-outs or the AA-pack problems.

## 4. Claude Code skills / plugins / connectors

Searches run: MCP registry ("arduino", "embedded/platformio/serial/wokwi/kicad", "electronics/pcb") returned nothing relevant. The plugin directory found **ESP32-AI-Agent-Skill** (community, ESP32 only) and **BoardRepo** (community, KiCad PCB review). The skills search returned nothing. **Wokwi MCP** exists as `wokwi-cli mcp`: "experimental", needs a token, and names Claude Code as a supported agent ([docs](https://docs.wokwi.com/wokwi-ci/mcp-support)). No Fritzing or PlatformIO connector was found.

| Tier | Item | Why |
|---|---|---|
| Must-have | `arduino-cli` via Bash (not an MCP) | compile/upload/monitor, §1 |
| Must-have | `superpowers:systematic-debugging` **or** `mattpocock-skills:diagnosing-bugs` (pick one) | hardware faults (noise, brown-outs, wrong thresholds) need hypothesis → measure loops |
| Must-have | `superpowers:verification-before-completion` | forces "compile output / CSV evidence" before claiming done; hardware claims still need the human |
| Must-have | `data:analyze` + `dataviz` (or `data:create-viz`) | plot logged CSVs: rest/squeeze distributions, threshold margins, false triggers |
| Nice | `mattpocock-skills:tdd` or `superpowers:test-driven-development` | only after the logic is extracted (§3) |
| Nice | `mattpocock-skills:wizard` | a guided script for human-only steps (electrodes, power-on order, calibration) |
| Nice | `anthropic-skills:learn`, `engineering:documentation`, `docx`/`pptx`, `pdf` | student understanding, college report/slides, reading the build-guide PDF |
| Nice | Wokwi MCP / wokwi-cli | only if you adopt Wokwi sims |
| Nice | `code-review`, `simplify`, `superpowers:writing-plans` | small firmware diffs; planning the §5 upgrade |
| Skip | GitHub plugin MCP | `gh` is already authenticated as Kirito0088 [verified], which covers issues |
| Skip | `arduino-claude-mcp` | broken wiring, compile-only (§1) |
| Skip | ESP32 skill, BoardRepo/KiCad | wrong board / no PCB in scope |
| Skip | `data:statistical-analysis`, `domain-modeling`, `prototype`, frontend/design/legal/product/sql plugins | not relevant to a 110-line sketch |

## 5. Is ML needed?

**Verdict: no.** The current goal is one channel and binary open/close on an Uno. Threshold control on an EMG envelope is the standard, sufficient approach. ML would add data collection and fragility for no functional gain.

- **What commercial hands actually use.** "In commercial control systems, the intensity of muscle activity is extracted from the EMG and used for single degrees of freedom activation (direct control)." Academic pattern recognition (PR) had not reached commercial systems. The reason given is the "relatively small functional improvement in daily situations" at the cost of robustness ([Farina et al. 2014, IEEE TNSRE 22:797](https://doi.org/10.1109/TNSRE.2014.2305111)). PR "has yet to transition to a clinically viable option" ([Scheme & Englehart 2011, JRRD 48:643](https://doi.org/10.1682/JRRD.2010.09.0177)). In a small 2-DOF crossover, PR and direct control were similar on 74% of metrics, and direct control was better on the remaining 26% ([Resnik et al. 2018, JNER 15](https://doi.org/10.1186/s12984-018-0361-3)).
- **What your sensor gives you.** The red board with +Vs/GND/−Vs/SIG and a dual supply matches the Advancer Technologies Muscle Sensor v3 [identification unverified]. That board "measures, filters, rectifies, and amplifies" and outputs 0 V to +Vs. It has no raw-EMG pin ([Pololu 2726](https://www.pololu.com/product/2726)). So the firmware already works on an envelope, which is the right input for thresholds. The flip side: time-domain PR features such as zero crossings and slope sign changes cannot be computed from an envelope.
- **What multi-gesture classification needs.**
  - Hudgins et al. extracted features from time segments of raw EMG and classified them with a neural network. That study showed more functions from **a single channel**, so one channel is not a hard limit ([Hudgins 1993, IEEE TBME 40:82](https://doi.org/10.1109/10.204774)).
  - The robust real-time scheme used **four channels** with continuous classification ([Englehart & Hudgins 2003, IEEE TBME 50:848](https://doi.org/10.1109/TBME.2003.813539)). The standard feature set is MAV, ZC, SSC, WL with an LDA classifier (feature/classifier details are from the paper bodies, not re-read here) [unverified in this pass]. For feature comparisons see [Phinyomark et al. 2012, ESWA 39:7420](https://doi.org/10.1016/j.eswa.2012.01.102).
  - Raw sEMG needs sampling well above the ~20 Hz high-pass region ([De Luca et al. 2010](https://doi.org/10.1016/j.jbiomech.2010.01.027) recommends a 20 Hz corner). Plan on ≥1 kHz per channel [rule of thumb, unverified].
- **Is TinyML feasible on an ATmega328P? No.**
  - TFLM/LiteRT-Micro "requires a 32-bit platform"; its core runtime "fits in 16 KB on an Arm Cortex M3" ([LiteRT Micro](https://developers.google.com/edge/litert/microcontrollers/overview); [David et al. 2021](https://arxiv.org/abs/2010.08678)). The Uno is 8-bit with 2 KB SRAM, and this sketch already uses 287 B.
  - Edge Impulse's Arduino list is Nano 33 BLE Sense, Nicla, Portenta, UNO Q, and VENTUNO Q. It does not include the Uno R3 or R4 ([EI hardware](https://docs.edgeimpulse.com/docs/edge-ai-hardware/edge-ai-hardware)).
  - A tiny classifier *could* run on AVR. emlearn is tested on "AVR Atmega (8 bit)" from ~2 kB flash ([emlearn](https://github.com/emlearn/emlearn)), and LDA is just dot products. On AVR the limit is the signal and channels, not compute.
- **Cheap non-ML improvements, in priority order.**
  1. Log state and raw ADC (§2) and tune from data.
  2. Add a dwell/debounce: require the level above `closeAt` for N consecutive samples (e.g. 50–100 ms). This works together with the existing hysteresis.
  3. Add slow adaptive baseline tracking while open and below `openAt`, to handle electrode drift and sweat.
  4. Add a re-calibrate trigger (serial command or button) that doesn't need a reset.
  5. Watch latency. The EMA (α=0.1 every ~5 ms) has a time constant of ~48 ms. `moveAll()` blocks for 300 ms with the servos staggered 100 ms apart. Keep the total well under the ~200 ms real-time criterion in Farina 2014 for the first finger.
  6. Optional proportional grip: map the level between `openAt` and the squeeze peak to an angle.

**Upgrade path**

| If you later want… | …then you need |
|---|---|
| Better binary control | Nothing new. The §5 non-ML list plus CSV tuning. |
| Proportional grip | Same Uno. Map the envelope to an angle and add rate limiting. |
| 2–3 gestures (e.g. open/power grip/pinch) | **Raw** EMG: MyoWare 2.0 has RAW/RECT/ENV outputs on a single 2.27–5.47 V supply ([SparkFun guide](https://learn.sparkfun.com/tutorials/getting-started-with-the-myoware-20-muscle-sensor-ecosystem/all)). Use 2–4 channels on flexor/extensor sites and time-domain features + LDA. That can run on an **Uno R4 Minima** (Cortex-M4 48 MHz, 32 KB SRAM, 5 V I/O, up to 14-bit ADC; [docs](https://docs.arduino.cc/hardware/uno-r4-minima/)). |
| TinyML (TFLM/Edge Impulse) | A 32-bit EI/TFLM board: **Nano 33 BLE Sense Rev2** (the Rev1 is EOL; [docs](https://docs.arduino.cc/hardware/nano-33-ble-sense/)) or **ESP32**. **Both are 3.3 V I/O, not 5 V tolerant** ([ABX00069 datasheet](https://docs.arduino.cc/resources/datasheets/ABX00069-datasheet.pdf)). Power the sensor from 3.3 V or divide its output, because Muscle Sensor v3 output reaches +Vs. |
| Training data | Extend the §2 logger. Stream raw ADC at a fixed rate in binary or compact CSV (≥1 kHz/channel exceeds what the 115200-baud text line can carry; raise the baud or pack the data). Add a `label` column you set from the PC (cue "rest/fist/pinch" on screen). Record several sessions with electrode re-placement, since PR's main failure mode is robustness ([Farina 2014](https://doi.org/10.1109/TNSRE.2014.2305111)). |

## 6. Hardware/safety references (brief)

- **Uno Rev3:** [product docs](https://docs.arduino.cc/hardware/uno-rev3/), [datasheet PDF](https://docs.arduino.cc/resources/datasheets/A000066-datasheet.pdf), [full pinout](https://arduino.cc/resources/pinouts/A000066-full-pinout.pdf). The toolchain reports 32256 B flash usable and 2048 B RAM [verified from compile output]. `analogRead` is 10-bit, 0–5 V, ~100 µs per read, so up to ~10 kHz ([Arduino reference source](https://github.com/arduino/reference-en/blob/master/Language/Functions/Analog%20IO/analogRead.adoc)).
- **Servo library 1.3.0** (header shipped in `%LOCALAPPDATA%\Arduino15\libraries\Servo`): "analogWrite of PWM on pins associated with the timer are disabled when the first servo is attached". On the Uno that timer is Timer1 (pins 9/10), from `src/avr/ServoTimers.h` [verified locally]. Pins 5/6 as *servo* pins are fine. Just don't also `analogWrite` pin 10. Reference: [arduino.cc Servo](https://www.arduino.cc/reference/en/libraries/servo/).
- **MG90S** ([Tower Pro](https://towerpro.com.tw/product/mg90s-3/)): operating voltage 4.8 V; stall torque 1.8 kg·cm @4.8 V, 2.2 @6.6 V; 0.10 s/60° @4.8 V. **Tower Pro publishes no stall current.** Treat any stall figure as unverified and measure it. Fresh 4×AA is ~6 V, inside the 4.8–6.6 V range quoted for torque.
- **EMG sensor output** goes up to +Vs ([Pololu](https://www.pololu.com/product/2726)). With +Vs = Uno 5 V, SIG stays within the ADC range. This matters on any 3.3 V upgrade board.

---

## Recommended setup checklist (run these yourself)

```powershell
# 1. arduino-cli on PATH (Arduino SA package; or skip and use the IDE-bundled exe)
winget install --id ArduinoSA.CLI -e
arduino-cli version
arduino-cli core install arduino:avr         # already installed via IDE (1.8.8); harmless no-op check
arduino-cli lib install Servo                # standalone CLI doesn't see the IDE's built-in libs
#   no-install alternative: $cli="C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
#   & $cli compile --config-file "$HOME\.arduinoIDE\arduino-cli.yaml" -b arduino:avr:uno firmware\somatic_arm

# 2. Compile, find port, upload, watch (close Serial Monitor/Plotter first)
arduino-cli compile -b arduino:avr:uno firmware\somatic_arm
arduino-cli board list                       # note COMx for the Uno
arduino-cli upload  -b arduino:avr:uno -p COMx firmware\somatic_arm
arduino-cli monitor -p COMx --config 115200 --quiet --timestamp

# 3. Logging
py -m pip install pyserial
py -m serial.tools.list_ports
py tools\log_emg.py COMx 60 session1.csv     # script in section 2

# 4. DONE 2026-09-27: .mcp.json and docs/agents/arduino-mcp.md removed; see docs/agents/arduino-cli.md
npm uninstall -g arduino-claude-mcp                   # optional

# 5. Host tests after extracting emg_logic.h (no install needed)
g++ -std=c++17 -I firmware\somatic_arm test\test_emg_logic.cpp -o test_emg_logic.exe; .\test_emg_logic.exe

# 6. Optional: Wokwi simulation (needs a free account token from the Wokwi CI dashboard)
iwr https://wokwi.com/ci/install.ps1 -useb | iex
$env:WOKWI_CLI_TOKEN="<token>"; wokwi-cli . --scenario calibrate.yaml
```
