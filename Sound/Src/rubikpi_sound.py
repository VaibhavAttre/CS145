#!/usr/bin/env python3


import os
import sys
import json
import time
import threading

# ---- third party -----------------------------------------------------------
try:
    import gpiod
except ImportError:
    sys.exit("python3-libgpiod missing.  Run: sudo apt-get install -y python3-libgpiod")

try:
    import smbus2
except ImportError:
    smbus2 = None   # EEPROM features will be disabled with a clear message


# ============================================================================
#  CONFIG
# ============================================================================
GPIO_CHIP   = "gpiochip4"     # confirmed: header lines are on gpiochip4 (f100000.pinctrl)
I2C_BUS     = 1               # confirmed: i2cdetect found 0x50 on i2c-1
EEPROM_ADDR = 0x50
MAX_SEQ_LEN = 32

CONFIG_PATH = os.path.expanduser("~/.rubikpi_sound_pins.json")

# Default LINE OFFSETS within gpiochip4. Confirm with --test, then saved to disk.
DEFAULT_PINS = {
    "ROW": [8, 24, 25, 32],     # ROW0..ROW3
    "COL": [26, 27, 44, 55],    # COL0..COL3
    "LED_PLAY":     33,
    "LED_RECORD":   34,
    "LED_PLAYBACK": 9,
    "BUZZER":       105,        # software-PWM line
}

KEYS = [
    ['1', '2', '3', 'A'],
    ['4', '5', '6', 'B'],
    ['7', '8', '9', 'C'],
    ['*', '0', '#', 'D'],
]

TONE_MAP = {
    '1': 262, '2': 294, '3': 330, '4': 349,
    '5': 392, '6': 440, '7': 494, '8': 523,
}

MODE_PLAY, MODE_RECORD, MODE_PLAYBACK = 'PLAY', 'RECORD', 'PLAYBACK'


def load_pins():
    pins = json.loads(json.dumps(DEFAULT_PINS))  # deep copy
    if os.path.exists(CONFIG_PATH):
        try:
            with open(CONFIG_PATH) as f:
                pins.update(json.load(f))
            print(f"[config] loaded pin map from {CONFIG_PATH}")
        except Exception as e:
            print(f"[config] could not read {CONFIG_PATH}: {e} (using defaults)")
    return pins


def save_pins(pins):
    try:
        with open(CONFIG_PATH, "w") as f:
            json.dump(pins, f, indent=2)
        print(f"[config] saved pin map to {CONFIG_PATH}")
    except Exception as e:
        print(f"[config] could not write {CONFIG_PATH}: {e}")


# ============================================================================
#  SOFTWARE PWM TONE  (real-time thread)
# ============================================================================
class TonePlayer:
    """
    Software PWM on a gpiod line, run from a dedicated thread.

    The kernel hardware PWM channel on this board is owned by another driver
    ("Device or resource busy"), so we bit-bang instead -- but cleanly:
      * a single persistent thread toggles the line (no per-note thread churn)
      * busy-wait timing with perf_counter (NOT time.sleep) so the half-period
        is held accurately even down at ~1 ms for the high notes
      * we bump the thread to SCHED_FIFO real-time priority if allowed, which
        massively reduces jitter vs a sleep-based version.
    """
    def __init__(self, line):
        self._line = line          # an already-requested gpiod output line
        self._freq = 0
        self._lock = threading.Lock()
        self._stop = threading.Event()
        self.ok = True
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _try_realtime(self):
        try:
            param = os.sched_param(10)
            os.sched_setscheduler(0, os.SCHED_FIFO, param)
        except Exception:
            pass  # not permitted -> normal scheduling

    def _run(self):
        self._try_realtime()
        pc = time.perf_counter
        while not self._stop.is_set():
            with self._lock:
                freq = self._freq
            if freq > 0:
                half = 0.5 / freq
                self._line.set_value(1)
                t = pc()
                while pc() - t < half:
                    pass
                self._line.set_value(0)
                t = pc()
                while pc() - t < half:
                    pass
            else:
                self._line.set_value(0)
                time.sleep(0.003)

    def play(self, hz):
        with self._lock:
            self._freq = hz

    def stop(self):
        with self._lock:
            self._freq = 0
        try:
            self._line.set_value(0)
        except Exception:
            pass

    def cleanup(self):
        self._stop.set()
        self._thread.join(timeout=1)
        try:
            self._line.set_value(0)
        except Exception:
            pass


# ============================================================================
#  GPIO via libgpiod  (one chip, lines requested by OFFSET)
# ============================================================================
class GpioBank:
    """
    Thin wrapper around libgpiod v1.x API (1.6.3).
    Rows = push-pull outputs driven HIGH when idle, pulled LOW one at a time
    during the scan. Columns = inputs with internal pull-up (active-low when
    pressed). Buzzer = output line driven by the TonePlayer thread.
    """
    def __init__(self, chipname, row_offsets, col_offsets, led_offsets, buzzer_offset):
        self._chip = gpiod.Chip(chipname)

        self._rows = []
        
        for off in row_offsets:
            ln = self._chip.get_line(off)
            ln.request(consumer="sound-row", type=gpiod.LINE_REQ_DIR_OUT,
                       default_vals=[0])
            self._rows.append(ln)

        try:
            pulldown = gpiod.LINE_REQ_FLAG_BIAS_PULL_DOWN
        except AttributeError:
            pulldown = 0
        self._cols = []
        for off in col_offsets:
            ln = self._chip.get_line(off)
            ln.request(consumer="sound-col", type=gpiod.LINE_REQ_DIR_IN,
                       flags=pulldown)
            self._cols.append(ln)

        self._leds = {}
        for name, off in led_offsets.items():
            ln = self._chip.get_line(off)
            ln.request(consumer="sound-led", type=gpiod.LINE_REQ_DIR_OUT,
                       default_vals=[0])
            self._leds[name] = ln

        # Buzzer line (driven by the software-PWM TonePlayer thread)
        self.buzzer_line = self._chip.get_line(buzzer_offset)
        self.buzzer_line.request(consumer="sound-buzz",
                                 type=gpiod.LINE_REQ_DIR_OUT, default_vals=[0])

    # ---- keypad ----
    def scan(self):
        # idle all rows LOW
        for r in self._rows:
            r.set_value(0)
        for r_idx, r in enumerate(self._rows):
            r.set_value(1)                    # drive THIS row high
            time.sleep(0.0015)                # let column lines settle
            for c_idx, c in enumerate(self._cols):
                if c.get_value() == 1:        # pressed: column pulled HIGH by the row
                    r.set_value(0)
                    return KEYS[r_idx][c_idx]
            r.set_value(0)
        return None

    # ---- leds ----
    def led(self, name, on):
        ln = self._leds.get(name)
        if ln:
            ln.set_value(1 if on else 0)

    def cleanup(self):
        try:
            for r in self._rows:
                r.set_value(0); r.release()
            for c in self._cols:
                c.release()
            for ln in self._leds.values():
                ln.set_value(0); ln.release()
            try:
                self.buzzer_line.set_value(0); self.buzzer_line.release()
            except Exception:
                pass
            self._chip.close()
        except Exception:
            pass


# ============================================================================
#  MODE LEDS  (uses the GpioBank led lines)
# ============================================================================
class ModeLEDs:
    def __init__(self, bank):
        self._bank = bank
        self._mode = MODE_PLAY
        self.set(MODE_PLAY)

    def set(self, mode):
        self._mode = mode
        self._bank.led("LED_PLAY",     mode == MODE_PLAY)
        self._bank.led("LED_RECORD",   mode == MODE_RECORD)
        self._bank.led("LED_PLAYBACK", mode == MODE_PLAYBACK)

    def get(self):
        return self._mode


# ============================================================================
#  I2C EEPROM  (smbus2 on i2c-1, identical layout/logic to the STM32 driver)
# ============================================================================
class EEPROM:
    WRITE_CYCLE_S = 0.006

    def __init__(self, bus_num, addr):
        self._addr = addr
        self._seq_len = 0
        self.ok = False
        if smbus2 is None:
            print("[EEPROM] smbus2 not installed (pip install smbus2). Record/Playback off.")
            return
        try:
            self._bus = smbus2.SMBus(bus_num)
            self._read(0)              # probe
            self.ok = True
            print(f"[EEPROM] OK on i2c-{bus_num} @ 0x{addr:02X}")
        except Exception as e:
            print(f"[EEPROM] not responding on i2c-{bus_num} @ 0x{addr:02X}: {e}")
            print("[EEPROM] Record/Playback disabled.")

    def _write(self, mem_addr, data):
        hi, lo = (mem_addr >> 8) & 0xFF, mem_addr & 0xFF
        self._bus.write_i2c_block_data(self._addr, hi, [lo, data])
        time.sleep(self.WRITE_CYCLE_S)

    def _read(self, mem_addr):
        hi, lo = (mem_addr >> 8) & 0xFF, mem_addr & 0xFF
        wr = smbus2.i2c_msg.write(self._addr, [hi, lo])
        rd = smbus2.i2c_msg.read(self._addr, 1)
        self._bus.i2c_rdwr(wr, rd)
        return list(rd)[0]

    def init(self):
        if not self.ok:
            return
        try:
            v = self._read(0)
            self._seq_len = 0 if v > MAX_SEQ_LEN else v
            if v > MAX_SEQ_LEN:
                self.clear()
        except Exception:
            self.clear()

    def clear(self):
        if not self.ok:
            return
        self._seq_len = 0
        try:
            self._write(0, 0)
        except Exception as e:
            print(f"[EEPROM] clear failed: {e}")

    def append(self, key):
        if not self.ok or self._seq_len >= MAX_SEQ_LEN:
            return
        try:
            self._write(1 + self._seq_len, ord(key))
            self._seq_len += 1
            self._write(0, self._seq_len)
        except Exception as e:
            print(f"[EEPROM] append failed: {e}")

    def read_sequence(self):
        if not self.ok:
            return []
        try:
            n = min(self._read(0), MAX_SEQ_LEN)
        except Exception:
            return []
        out = []
        for i in range(n):
            try:
                out.append(chr(self._read(1 + i)))
            except Exception:
                out.append('1')
        self._seq_len = n
        return out

    def close(self):
        try:
            self._bus.close()
        except Exception:
            pass


# ============================================================================
#  PLAYBACK
# ============================================================================
def play_sequence(eeprom, tone):
    seq = eeprom.read_sequence()
    if not seq:
        print("[Playback] nothing recorded.")
        return
    for key in seq:
        freq = TONE_MAP.get(key, 0)
        if freq:
            tone.play(freq)
            time.sleep(1.0)
            tone.stop()
            time.sleep(0.05)


# ============================================================================
#  INTERACTIVE PIN TEST / DISCOVERY
# ============================================================================
def test_mode(pins):
   
    print("\n=== PIN TEST MODE ===")
    print(f"Chip: {GPIO_CHIP}.  Edit offsets until hardware responds correctly.")
    chip = gpiod.Chip(GPIO_CHIP)

    def blink(offset, label):
        try:
            ln = chip.get_line(offset)
            ln.request(consumer="test", type=gpiod.LINE_REQ_DIR_OUT, default_vals=[0])
            print(f"  -> blinking {label} on offset {offset} 5x (watch the LED)...")
            for _ in range(5):
                ln.set_value(1); time.sleep(0.25)
                ln.set_value(0); time.sleep(0.25)
            ln.release()
        except Exception as e:
            print(f"  !! offset {offset} could not be driven: {e} (already used by a driver?)")

    for label in ("LED_PLAY", "LED_RECORD", "LED_PLAYBACK"):
        ans = input(f"\nTest {label} (current offset {pins[label]})? [Enter=yes / number=new offset / s=skip] ").strip()
        if ans.lower() == 's':
            continue
        if ans.isdigit():
            pins[label] = int(ans)
        blink(pins[label], label)
        if input(f"  Did the correct {label} LED blink? [y/N] ").strip().lower() != 'y':
            new = input(f"  Enter correct offset for {label} (or Enter to keep {pins[label]}): ").strip()
            if new.isdigit():
                pins[label] = int(new)

    print("\nNote: buzzer uses software PWM on offset %d (a real-time thread)." % pins["BUZZER"])
    print("If no sound later, confirm that offset is your buzzer pin.")

    chip.close()
    save_pins(pins)
    print("=== done.  Run without --test to start the keypad. ===\n")


# ============================================================================
#  MAIN
# ============================================================================
def main():
    pins = load_pins()

    if "--reset-config" in sys.argv:
        if os.path.exists(CONFIG_PATH):
            os.remove(CONFIG_PATH)
        print("[config] reset to defaults.")
        return

    if "--test" in sys.argv:
        test_mode(pins)
        return

    print("Initialising hardware...")

    led_offsets = {
        "LED_PLAY":     pins["LED_PLAY"],
        "LED_RECORD":   pins["LED_RECORD"],
        "LED_PLAYBACK": pins["LED_PLAYBACK"],
    }
    try:
        bank = GpioBank(GPIO_CHIP, pins["ROW"], pins["COL"],
                        led_offsets, pins["BUZZER"])
    except Exception as e:
        sys.exit(f"GPIO init failed: {e}\nRun  sudo python3 {sys.argv[0]} --test  to fix pin offsets.")
    print("  [OK] Keypad + LEDs")

    tone = TonePlayer(bank.buzzer_line)
    print("  [OK] Buzzer (software PWM, real-time thread)")

    leds = ModeLEDs(bank)
    eeprom = EEPROM(I2C_BUS, EEPROM_ADDR)
    eeprom.init()

    leds.set(MODE_PLAY)

    # Debounce: a reading must repeat DEBOUNCE_N times in a row before we
    # treat it as the real, stable key state. This stops the scan flicker
    # that caused tones to stutter and not stop cleanly on release.
    DEBOUNCE_N = 3
    raw_prev   = None      # last raw scan result
    raw_count  = 0         # how many times in a row we've seen raw_prev
    stable_key = None      # the current debounced key (None = nothing held)
    last_stable = None     # previous debounced key, to detect transitions

    print("\nReady!")
    print("  * -> Play    0 -> Record (clears)    # -> Playback")
    print("  Keys 1-8 play notes.  Press 9 in Playback to replay.")
    if not eeprom.ok:
        print("  *** EEPROM unavailable - Record/Playback off ***")
    print("  Ctrl-C to quit.\n")

    try:
        while True:
            raw = bank.scan()

            # ---- debounce: only accept a value seen DEBOUNCE_N times ----
            if raw == raw_prev:
                raw_count += 1
            else:
                raw_prev  = raw
                raw_count = 1
            if raw_count < DEBOUNCE_N:
                time.sleep(0.003)
                continue
            stable_key = raw_prev      # confirmed stable

            # Nothing changed since last confirmed state -> keep holding.
            if stable_key == last_stable:
                time.sleep(0.005)
                continue

            # ---- a real press/release transition happened ----
            key  = stable_key
            mode = leds.get()

            # KEY RELEASED (or moved to a non-tone key): stop any tone.
            if key is None or TONE_MAP.get(key) is None:
                if TONE_MAP.get(last_stable):     # we were playing a note
                    tone.stop()

            if key == '*':
                tone.stop(); leds.set(MODE_PLAY)
                print("[Mode] PLAY")

            elif key == '0':
                tone.stop(); eeprom.clear(); leds.set(MODE_RECORD)
                print("[Mode] RECORD  (cleared)")

            elif key == '#':
                tone.stop(); leds.set(MODE_PLAYBACK)
                print("[Mode] PLAYBACK")

            elif mode == MODE_PLAYBACK:
                if key == '9':
                    print("[Playback] replaying...")
                    play_sequence(eeprom, tone)
                    print("[Playback] done.")

            else:
                freq = TONE_MAP.get(key, 0)
                if freq:
                    tone.play(freq)               # starts and HOLDS until release
                    print(f"[Note] {key} -> {freq} Hz")
                    if mode == MODE_RECORD:
                        eeprom.append(key)
                        print(f"[Record] saved '{key}'")

            last_stable = key
            time.sleep(0.005)

    except KeyboardInterrupt:
        print("\nShutting down...")
    finally:
        tone.cleanup()
        bank.cleanup()
        eeprom.close()
        print("Done.")


if __name__ == "__main__":
    main()