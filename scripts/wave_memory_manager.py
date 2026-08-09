"""
Module: wave_memory_manager.py
Description: Drop-in orchestration module for managing the wave-mother benchmarking suite.
"""

import os
from pathlib import Path
from dataclasses import dataclass, field
from typing import List, Dict

@dataclass
class EllipticalMemoryState:
    active_sources: List[str] = field(default_factory=list)
    historical_logs: List[str] = field(default_factory=list)
    orchestrators: List[str] = field(default_factory=list)

class WaveMotherManager:
    def __init__(self, workspace_root: str = "."):
        self.root = Path(workspace_root)
        self.memory = EllipticalMemoryState()
        
    def index_workspace(self, file_list: List[str]) -> Dict[str, int]:
        for file in file_list:
            ext = Path(file).suffix
            if ext == '.c':
                self.memory.active_sources.append(file)
            elif ext in ['.py', '.sh']:
                self.memory.orchestrators.append(file)
            elif ext == '.csv':
                self.memory.historical_logs.append(file)
        return {
            "sources_indexed": len(self.memory.active_sources),
            "logs_indexed": len(self.memory.historical_logs),
            "orchestrators_indexed": len(self.memory.orchestrators)
        }

    def get_latest_trajectory_data(self) -> List[str]:
        return [f for f in self.memory.historical_logs if 'trajectory' in f or 'learning' in f]

if __name__ == "__main__":
    import subprocess
    # Use real git file list
    res = subprocess.run(['git','ls-files'], capture_output=True, text=True)
    files = res.stdout.strip().split('\n')
    manager = WaveMotherManager()
    stats = manager.index_workspace(files)
    print(f"Workspace Memory Loaded: {stats}")
    print(f"Active C sources: {len(manager.memory.active_sources)}")
    print(f"Orchestrators: {len(manager.memory.orchestrators)}")
    print(f"Historical logs: {len(manager.memory.historical_logs)}")
    if manager.memory.historical_logs:
        print("Latest trajectory/learning files:", manager.get_latest_trajectory_data())
