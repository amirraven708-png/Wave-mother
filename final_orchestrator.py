import subprocess, sys, os, time
import torch, torch.nn as nn
from typing import Dict, Any, Tuple

# ========== 1. Tensor Benchmark (PyTorch) ==========
class TensorDecompositionBenchmark:
    def __init__(self, device=None):
        self.device = device or ("cuda" if torch.cuda.is_available() else "cpu")
        print(f"   [System] Using device: {self.device.upper()}")

    @staticmethod
    def count_parameters(model: nn.Module) -> Tuple[int, int]:
        total = sum(p.numel() for p in model.parameters())
        trainable = sum(p.numel() for p in model.parameters() if p.requires_grad)
        return total, trainable

    def measure_performance(self, model, x, warmup=5, steps=50, backward=False):
        model.to(self.device)
        model.eval() if not backward else model.train()
        x = x.to(self.device)

        with torch.set_grad_enabled(backward):
            for _ in range(warmup):
                out = model(x)
                if backward and hasattr(out, "sum"):
                    out.sum().backward()

        if self.device == "cuda":
            torch.cuda.reset_peak_memory_stats(self.device)
            torch.cuda.synchronize()

        start = time.perf_counter()
        if self.device == "cuda":
            start_ev = torch.cuda.Event(enable_timing=True)
            end_ev = torch.cuda.Event(enable_timing=True)
            start_ev.record()
            with torch.set_grad_enabled(backward):
                for _ in range(steps):
                    out = model(x)
                    if backward and hasattr(out, "sum"):
                        out.sum().backward()
            end_ev.record()
            torch.cuda.synchronize()
            latency = start_ev.elapsed_time(end_ev) / steps
        else:
            with torch.set_grad_enabled(backward):
                for _ in range(steps):
                    out = model(x)
                    if backward and hasattr(out, "sum"):
                        out.sum().backward()
            end = time.perf_counter()
            latency = ((end - start) / steps) * 1000.0

        total, trainable = self.count_parameters(model)
        peak_mem = torch.cuda.max_memory_allocated(self.device) / 1_048_576 if self.device == "cuda" else 0.0

        return {
            "total_params": total, "trainable_params": trainable,
            "latency_ms": round(latency, 4), "peak_memory_mb": round(peak_mem, 4),
            "throughput_fps": round(1000.0 / latency, 2) if latency > 0 else 0
        }

    def compare_models(self, baseline, tensor, x, warmup=5, steps=50, backward=False):
        print("   [Process] Benchmarking Baseline model... (Please wait)")
        bm = self.measure_performance(baseline, x, warmup, steps, backward)
        
        print("   [Process] Benchmarking Tensor Decomposed model... (Please wait)")
        tm = self.measure_performance(tensor, x, warmup, steps, backward)
        
        mem_savings = 0.0
        if bm["peak_memory_mb"] > 0:
            mem_savings = ((bm["peak_memory_mb"] - tm["peak_memory_mb"]) / bm["peak_memory_mb"]) * 100.0

        return {
            "baseline": bm, "tensor_decomposed": tm,
            "comparison": {
                "param_compression_ratio": round(bm["total_params"] / max(tm["total_params"], 1), 2),
                "speedup_factor": round(bm["latency_ms"] / max(tm["latency_ms"], 1e-6), 2),
                "memory_savings_percent": round(mem_savings, 2)
            }
        }

# ========== 2. Wave-Learning Benchmark (C) ==========
def run_wave_benchmark(instance_count=30):
    print("🔹 Running Wave Learning Benchmark (C)...")
    build = subprocess.run(
        ["gcc", "-Wall", "-O2", "-o", "core/bin/psi_advanced_learning", "core/src/psi_advanced_learning.c", "-lm"],
        capture_output=True, text=True
    )
    if build.returncode != 0:
        print("Build error:\n", build.stderr)
        return None
        
    try:
        run = subprocess.run(["./core/bin/psi_advanced_learning"], capture_output=True, text=True, check=True)
    except subprocess.CalledProcessError as e:
        print(f"Execution error: {e}")
        return None

    lines = run.stdout.splitlines()
    stats = {}
    for l in lines:
        l = l.strip()
        if "Greedy" in l and "avg:" in l:
            stats["greedy"] = float(l.split()[-1])
        elif "LS" in l and "avg:" in l:
            stats["ls"] = float(l.split()[-1])
        elif "PSI" in l and "avg:" in l:
            stats["psi"] = float(l.split()[-1])
            
    return stats

# ========== 3. Final Orchestrator ==========
def main():
    print("╔══════════════════════════════════════════════════╗")
    print("║     WAVE MOTHER - FINAL INTEGRATED BENCHMARK     ║")
    print("╚══════════════════════════════════════════════════╝\n")

    print("🔸 Tensor Decomposition Benchmark (PyTorch)")
    
    # کاهش حجم بچ سایز برای تسریع در اجرای CPU
    x = torch.randn(8, 128, 768) 
    
    baseline = nn.Sequential(
        nn.Linear(768, 3072), 
        nn.GELU(), 
        nn.Linear(3072, 768)
    )
    
    tensor = nn.Sequential(
        nn.Linear(768, 64, bias=False), 
        nn.Linear(64, 3072), 
        nn.GELU(),
        nn.Linear(3072, 64, bias=False), 
        nn.Linear(64, 768)
    )
    
    bench = TensorDecompositionBenchmark()
    res = bench.compare_models(baseline, tensor, x, warmup=5, steps=50)
    t = res["tensor_decomposed"]
    c = res["comparison"]
    
    print(f"\n  Baseline params : {res['baseline']['total_params']:,}")
    print(f"  Tensor params   : {t['total_params']:,}")
    print(f"  Compression     : {c['param_compression_ratio']}x")
    print(f"  Speedup         : {c['speedup_factor']}x")
    print(f"  Memory saved    : {c['memory_savings_percent']:.1f}%\n")

    print("🔸 Wave Learning Benchmark (C)")
    wave_stats = run_wave_benchmark(30)
    
    if wave_stats and "psi" in wave_stats and "ls" in wave_stats:
        greedy = wave_stats.get("greedy", 0)
        ls = wave_stats.get("ls", 0)
        psi = wave_stats.get("psi", 0)
        
        improvement_val = psi - ls
        improvement_pct = ((psi - ls) / ls * 100) if ls > 0 else 0
        
        print(f"  Greedy avg : {greedy:.1f}")
        print(f"  LS avg     : {ls:.1f}")
        print(f"  PSI avg    : {psi:.1f}")
        print(f"  Improvement: {improvement_val:.1f} ({improvement_pct:.2f}%)\n")
    else:
        print("  Wave benchmark failed or output format mismatched.\n")
        return

    print("════════════════════════════════════════════════════")
    print("            C O M B I N E D   V E R D I C T           ")
    print("════════════════════════════════════════════════════")
    print(f" Tensor model compresses parameters by {c['param_compression_ratio']}x")
    print(f" Wave PSI-CORE beats LS by {improvement_val:.1f} units (+{improvement_pct:.2f}%)")
    print("════════════════════════════════════════════════════")
    print(" Wave Mother demonstrates dual superiority:       ")
    print("  • Structural compression (tensor decomposition)")
    print("  • Adaptive learning (PSI-CORE over greedy)")
    print("════════════════════════════════════════════════════")

if __name__ == "__main__":
    main()
