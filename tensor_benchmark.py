import time
import torch
import torch.nn as nn
from typing import Dict, Any, Tuple

class TensorDecompositionBenchmark:
    def __init__(self, device: str = None):
        self.device = device if device else ("cuda" if torch.cuda.is_available() else "cpu")

    @staticmethod
    def count_parameters(model: nn.Module) -> Tuple[int, int]:
        total_params = sum(p.numel() for p in model.parameters())
        trainable_params = sum(p.numel() for p in model.parameters() if p.requires_grad)
        return total_params, trainable_params

    def measure_performance(
        self,
        model: nn.Module,
        input_tensor: torch.Tensor,
        warmup_steps: int = 20,
        eval_steps: int = 100,
        run_backward: bool = False
    ) -> Dict[str, Any]:
        model.to(self.device)
        model.eval() if not run_backward else model.train()
        x = input_tensor.to(self.device)

        with torch.set_grad_enabled(run_backward):
            for _ in range(warmup_steps):
                out = model(x)
                if run_backward and hasattr(out, "sum"):
                    out.sum().backward()

        if self.device == "cuda":
            torch.cuda.reset_peak_memory_stats(self.device)
            torch.cuda.synchronize()

        start_time = time.perf_counter()
        
        if self.device == "cuda":
            start_event = torch.cuda.Event(enable_timing=True)
            end_event = torch.cuda.Event(enable_timing=True)
            start_event.record()

            with torch.set_grad_enabled(run_backward):
                for _ in range(eval_steps):
                    out = model(x)
                    if run_backward and hasattr(out, "sum"):
                        out.sum().backward()

            end_event.record()
            torch.cuda.synchronize()
            avg_latency_ms = start_event.elapsed_time(end_event) / eval_steps
        else:
            with torch.set_grad_enabled(run_backward):
                for _ in range(eval_steps):
                    out = model(x)
                    if run_backward and hasattr(out, "sum"):
                        out.sum().backward()
            end_time = time.perf_counter()
            avg_latency_ms = ((end_time - start_time) / eval_steps) * 1000.0

        peak_memory_mb = 0.0
        if self.device == "cuda":
            peak_memory_mb = torch.cuda.max_memory_allocated(self.device) / (1024 ** 2)

        total_params, trainable_params = self.count_parameters(model)

        return {
            "total_params": total_params,
            "trainable_params": trainable_params,
            "latency_ms": round(avg_latency_ms, 4),
            "peak_memory_mb": round(peak_memory_mb, 4),
            "throughput_fps": round(1000.0 / avg_latency_ms, 2) if avg_latency_ms > 0 else 0
        }

    def compare_models(
        self,
        baseline_model: nn.Module,
        tensor_model: nn.Module,
        input_tensor: torch.Tensor,
        warmup_steps: int = 20,
        eval_steps: int = 100,
        run_backward: bool = False
    ) -> Dict[str, Any]:
        base_metrics = self.measure_performance(
            baseline_model, input_tensor, warmup_steps, eval_steps, run_backward
        )
        tensor_metrics = self.measure_performance(
            tensor_model, input_tensor, warmup_steps, eval_steps, run_backward
        )

        param_compression = base_metrics["total_params"] / max(tensor_metrics["total_params"], 1)
        speedup_factor = base_metrics["latency_ms"] / max(tensor_metrics["latency_ms"], 1e-6)
        memory_savings_percent = 0.0
        
        if base_metrics["peak_memory_mb"] > 0:
            memory_savings_percent = (
                (base_metrics["peak_memory_mb"] - tensor_metrics["peak_memory_mb"]) 
                / base_metrics["peak_memory_mb"]
            ) * 100.0

        return {
            "baseline": base_metrics,
            "tensor_decomposed": tensor_metrics,
            "comparison": {
                "param_compression_ratio": round(param_compression, 2),
                "speedup_factor": round(speedup_factor, 2),
                "memory_savings_percent": round(memory_savings_percent, 2)
            }
        }


def print_benchmark_report(results: Dict[str, Any]) -> None:
    base = results["baseline"]
    tens = results["tensor_decomposed"]
    comp = results["comparison"]

    print("=" * 60)
    print("                گزارش بنچمارک مقایسه‌ای                 ")
    print("=" * 60)
    print(f"{'شاخص':<25} | {'مدل بیس‌لاین':<15} | {'مدل تنسوری':<15}")
    print("-" * 60)
    print(f"{'تعداد پارامترها':<25} | {base['total_params']:<15,} | {tens['total_params']:<15,}")
    print(f"{'تاخیر (ms)':<25} | {base['latency_ms']:<15} | {tens['latency_ms']:<15}")
    print(f"{'نرخ پردازش (FPS)':<25} | {base['throughput_fps']:<15} | {tens['throughput_fps']:<15}")
    print(f"{'حداکثر حافظه (MB)':<25} | {base['peak_memory_mb']:<15} | {tens['peak_memory_mb']:<15}")
    print("=" * 60)
    print(f"ضریب فشرده‌سازی پارامتر: {comp['param_compression_ratio']}x")
    print(f"ضریب تسریع اجرا (Speedup): {comp['speedup_factor']}x")
    print(f"میزان صرفه‌جویی در حافظه: {comp['memory_savings_percent']}%")
    print("=" * 60)


if __name__ == "__main__":
    batch_size = 32
    seq_len = 128
    d_in = 768
    d_out = 3072

    x_input = torch.randn(batch_size, seq_len, d_in)

    baseline_layer = nn.Sequential(
        nn.Linear(d_in, d_out),
        nn.GELU(),
        nn.Linear(d_out, d_in)
    )

    rank = 64
    tensor_layer = nn.Sequential(
        nn.Linear(d_in, rank, bias=False),
        nn.Linear(rank, d_out),
        nn.GELU(),
        nn.Linear(d_out, rank, bias=False),
        nn.Linear(rank, d_in)
    )

    benchmarking = TensorDecompositionBenchmark()
    res = benchmarking.compare_models(
        baseline_model=baseline_layer,
        tensor_model=tensor_layer,
        input_tensor=x_input,
        eval_steps=200
    )

    print_benchmark_report(res)
