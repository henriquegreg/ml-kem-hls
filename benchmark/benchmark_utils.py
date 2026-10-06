import os
import glob
import time
import ctypes
import threading
import subprocess
import numpy as np

def get_clock_mhz(xclbin_path: str) -> float:
    result = subprocess.run(
        ["xclbinutil", "--info", "--input", xclbin_path],
        capture_output=True,
        text=True,
    )

    lines = result.stdout.splitlines()
    in_scalable_clocks = False
    is_kernel_clk = False

    for line in lines:
        stripped = line.strip()

        if "Scalable clocks" in stripped:
            in_scalable_clocks = True
            continue

        if in_scalable_clocks:
            if stripped.startswith("Name:") and "KERNEL_CLK" in stripped:
                is_kernel_clk = True
            elif stripped.startswith("Type:") and "KERNEL" in stripped:
                is_kernel_clk = True
            elif stripped.startswith("Name:") and "KERNEL_CLK" not in stripped:
                is_kernel_clk = False

            if is_kernel_clk and stripped.startswith("Frequency:"):
                match = re.search(r"(\d+(\.\d+)?)", stripped)
                if match:
                    return float(match.group(1))
    return 500.0
    
def get_cpu_mhz() -> float:
    try:
        with open("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq") as f:
            return float(f.read().strip()) / 1000.0
    except Exception:
        pass
    
    max_mhz = 0.0
    try:
        with open("/proc/cpuinfo") as f:
            for line in f:
                if "cpu MHz" in line:
                    freq = float(line.split(":")[1].strip())
                    max_mhz = max(max_mhz, freq)
    except Exception:
        pass
        
    return max_mhz if max_mhz > 0 else 1000.0

RAPL_PATH = "/sys/class/powercap/intel-rapl/intel-rapl:0/energy_uj"

def read_cpu_temp() -> float:
    try:
        with open("/sys/class/thermal/thermal_zone0/temp") as f:
            return int(f.read()) / 1000.0
    except Exception:
        pass
    try:
        import psutil
        for key in ("coretemp", "k10temp", "cpu_thermal"):
            sensors = psutil.sensors_temperatures().get(key, [])
            if sensors:
                return sensors[0].current
    except Exception:
        pass
    return float('nan')


def read_rapl_energy_uj() -> float:
    try:
        with open(RAPL_PATH) as f:
            return float(f.read())
    except Exception:
        return float('nan')

def find_fpga_hwmon():
    for hwmon in glob.glob("/sys/class/hwmon/hwmon*"):
        try:
            with open(os.path.join(hwmon, "device", "vendor"), "r") as f:
                vendor_id = f.read().strip()
                if vendor_id == "0x10ee":
                    return hwmon
        except Exception:
            continue
    return None

FPGA_HWMON_PATH = find_fpga_hwmon()

def read_fpga_sysfs() -> tuple[float, float]:
    if not FPGA_HWMON_PATH:
        return float('nan'), float('nan')

    temp_c = float('nan')
    power_w = float('nan')

    try:
        with open(os.path.join(FPGA_HWMON_PATH, "temp1_input"), "r") as f:
            temp_c = float(f.read().strip()) / 1000.0
    except Exception:
        pass

    try:
        with open(os.path.join(FPGA_HWMON_PATH, "power1_input"), "r") as f:
            power_w = float(f.read().strip()) / 1_000_000.0
    except Exception:
        pass

    return temp_c, power_w

class SensorSampler:
    def __init__(self, platform: str, interval_s: float = 0.01): 
        self.platform  = platform
        self.interval  = interval_s
        self.temps     = []
        self.powers    = [] 
        self._stop     = threading.Event()

    def start(self):
        self._stop.clear()
        
        if self.platform == "hardware":
            t, p = read_fpga_sysfs()
            if not np.isnan(p): self.powers.append(p)
            if not np.isnan(t): self.temps.append(t)
        else:
            self._last_e = read_rapl_energy_uj()
            self._last_t = time.perf_counter()
            t = read_cpu_temp()
            if not np.isnan(t): self.temps.append(t)
            
        self._thread = threading.Thread(target=self._loop, daemon=True)
        self._thread.start()

    def stop(self) -> tuple[list, list]:
        self._stop.set()
        self._thread.join()
        return self.temps, self.powers

    def _loop(self):
        while not self._stop.is_set():
            if self.platform == "hardware":
                t, p = read_fpga_sysfs() 
                if not np.isnan(p): self.powers.append(p)
                if not np.isnan(t): self.temps.append(t)
            else:
                t = read_cpu_temp()
                if not np.isnan(t): self.temps.append(t)
                
                e_now = read_rapl_energy_uj()
                t_now = time.perf_counter()
                dt = t_now - self._last_t
                if dt > 0:
                    de_j = (e_now - self._last_e) / 1e6
                    if de_j < 0: de_j = e_now / 1e6
                    self.powers.append(de_j / dt)
                self._last_e = e_now
                self._last_t = t_now
                    
            time.sleep(self.interval)

def load_kat_seeds(path: str, seed_len: int = 64) -> list[np.ndarray]:
    seeds = []
    with open(path, "r") as f:
        for line in f:
            line = line.strip()
            if not line or "=" not in line:
                continue
            hex_str = line.split("=", 1)[1].strip()
            seed_bytes = bytes.fromhex(hex_str)
            if len(seed_bytes) != seed_len:
                raise ValueError(
                    f"Seed with unexpected size: expected {seed_len} bytes, "
                    f"got {len(seed_bytes)} bytes."
                )
            seeds.append(np.frombuffer(seed_bytes, dtype=np.uint8).copy())
    return seeds