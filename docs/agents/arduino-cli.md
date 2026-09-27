# Arduino toolchain (arduino-cli)

Agents compile, upload and read the arm's firmware with `arduino-cli`, run through the shell. There is no Arduino MCP server. The one tried earlier (`arduino-claude-mcp`) was compile-only and wired up wrong. See `docs/research/tooling-skills-connectors-ml.md` §1.

## Which executable

`arduino-cli` is not on PATH. Use the copy bundled with Arduino IDE 2, and always pass the IDE's config file. Without it the IDE's built-in libraries are invisible and the build fails with `Servo.h: No such file or directory`.

```powershell
$cli = "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
$cfg = "$HOME\.arduinoIDE\arduino-cli.yaml"
```

If the user installs the standalone CLI (`winget install --id ArduinoSA.CLI -e`), plain `arduino-cli` works too. Run `arduino-cli lib install Servo` once first, because the standalone CLI doesn't see the IDE's libraries.

## Commands

Board FQBN: `arduino:avr:uno`. Sketch: `firmware\somatic_arm`.

```powershell
& $cli compile --config-file $cfg -b arduino:avr:uno firmware\somatic_arm
& $cli board list --config-file $cfg
& $cli upload  --config-file $cfg -b arduino:avr:uno -p COMx firmware\somatic_arm
& $cli monitor --config-file $cfg -p COMx --config 115200 --quiet --timestamp
```

- **Compile** needs no hardware. Agents can run it freely to check a firmware change.
- **Upload and monitor** need the Uno plugged in, so ask the user first. The build guide's rule applies: upload only with the electrodes off the arm, or with the laptop charger unplugged.
- **Only one program can hold the COM port.** Close the IDE's Serial Monitor/Plotter and stop any `monitor` or logger before uploading.
- **`monitor` never exits on its own.** Run it in the background with a timeout.
- **Opening the port usually resets the Uno** (unverified). That reruns `setup()`, including the servo wiring check and the 6 s relax/squeeze calibration, so the person wearing the electrodes has to follow the LED again.

## Logging EMG sessions

`tools/log_emg.py` saves the firmware's `emg:… close:… open:…` serial lines to CSV. It needs `pyserial` (`py -m pip install pyserial`).

```powershell
py -m serial.tools.list_ports          # find COMx
py tools\log_emg.py COMx 60 session1.csv
```
