"""
Module: wave_memory_parser.py
Description: Data parsing extension for the Wave Mother orchestrator to analyze benchmark CSVs.
"""

import csv
from pathlib import Path
from typing import List, Dict, Union, Optional

class WaveMemoryParser:
    def __init__(self, workspace_root: str = "."):
        self.root = Path(workspace_root)

    def parse_knapsack_comparison(self, filename: str = "knapsack_method_comparison.csv") -> List[Dict[str, Union[str, float]]]:
        filepath = self.root / filename
        if not filepath.exists():
            raise FileNotFoundError(f"Memory focus missing: {filepath} not found.")

        results = []
        with open(filepath, mode='r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            for row in reader:
                parsed_row = {}
                for key, value in row.items():
                    try:
                        parsed_row[key] = float(value) if '.' in value else int(value)
                    except ValueError:
                        parsed_row[key] = value
                results.append(parsed_row)
        
        return results

    def parse_psi_trajectory(self, filename: str) -> Dict[str, Union[List[float], float]]:
        filepath = self.root / filename
        if not filepath.exists():
            raise FileNotFoundError(f"Memory focus missing: {filepath} not found.")

        trajectory_data: Dict[str, List[float]] = {}
        
        with open(filepath, mode='r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            headers = reader.fieldnames or []
            
            for header in headers:
                trajectory_data[header] = []
                
            for row in reader:
                for header in headers:
                    val = row.get(header, "0")
                    try:
                        trajectory_data[header].append(float(val))
                    except ValueError:
                        pass

        summary = {"data": trajectory_data, "bounds": {}}
        for header, values in trajectory_data.items():
            if values:
                summary["bounds"][header] = {
                    "min": min(values),
                    "max": max(values),
                    "final_state": values[-1]
                }
                
        return summary

    def analyze_trajectory_stability(self, trajectory_summary: Dict) -> bool:
        bounds = trajectory_summary.get("bounds", {})
        
        for key in bounds:
            if 'error' in key.lower() or 'loss' in key.lower():
                b = bounds[key]
                range_span = b['max'] - b['min']
                if range_span == 0:
                    return True
                return (b['final_state'] - b['min']) / range_span < 0.10
                
        return False

if __name__ == "__main__":
    parser = WaveMemoryParser()
    
    try:
        # 1. Analyze constraints and local search methods
        knapsack_data = parser.parse_knapsack_comparison()
        print(f"Loaded {len(knapsack_data)} method comparisons.")
        
        # 2. Analyze wave trajectory
        trajectory_summary = parser.parse_psi_trajectory("psi_trajectory.csv")
        is_stable = parser.analyze_trajectory_stability(trajectory_summary)
        print(f"Trajectory Stable: {is_stable}")
        
    except FileNotFoundError as e:
        print(e)
