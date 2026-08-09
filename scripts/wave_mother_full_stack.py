import threading, time, subprocess, sys, os
sys.path.insert(0, os.path.dirname(__file__))
from wave_memory_parser import WaveMemoryParser
from wave_seqlock_writer import WaveSharedWriter

def heuristic_layer(writer, parser, interval=1.0):
    print("[Heuristic] Monitoring trajectory streams...")
    bid = 0
    while True:
        try:
            trajectory = parser.parse_psi_trajectory("psi_trajectory.csv")
            bounds = trajectory.get("bounds", {})
            phase_bounds = bounds.get("phase", {"min": -1.0, "max": 1.0})
            bid += 1
            for nid in range(10):
                writer.write_boundary(nid, {
                    "boundary_id": bid,
                    "min_phase_amplitude": phase_bounds["min"],
                    "max_phase_amplitude": phase_bounds["max"],
                    "resonance_threshold": 0.05,
                    "active_constraint_flags": 1
                })
        except FileNotFoundError:
            pass
        except Exception as e:
            print(f"[Heuristic] {e}")
        time.sleep(interval)

def main():
    print("🌊 Wave Mother Full Stack")
    parser = WaveMemoryParser(".")
    shm_path = os.path.join(os.path.expanduser("~"), "wave_shm.dat")
    writer = WaveSharedWriter(shm_path, 10)
    writer.set_active_nodes(10)

    threading.Thread(target=heuristic_layer, args=(writer, parser), daemon=True).start()
    print("[Core] Launching C fabric...")
    try:
        proc = subprocess.Popen(["./core/bin/super_compute_loop"])
        while proc.poll() is None: time.sleep(0.5)
    except KeyboardInterrupt:
        print("\n[Core] Shutting down...")
        if 'proc' in locals(): proc.terminate()
    finally:
        writer.close()
        print("[Core] Exit complete.")

if __name__ == "__main__":
    main()
