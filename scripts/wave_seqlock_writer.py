import os, mmap, ctypes
from typing import Dict, Union

class TrajectoryBoundary(ctypes.Structure):
    _fields_ = [
        ("boundary_id", ctypes.c_uint64),
        ("min_phase_amplitude", ctypes.c_double),
        ("max_phase_amplitude", ctypes.c_double),
        ("resonance_threshold", ctypes.c_double),
        ("active_constraint_flags", ctypes.c_uint32),
    ]

class SeqlockBoundary(ctypes.Structure):
    _fields_ = [
        ("sequence_counter", ctypes.c_uint32),
        ("data", TrajectoryBoundary)
    ]

class TrajectorySharedState(ctypes.Structure):
    _fields_ = [
        ("active_node_count", ctypes.c_uint32),
        ("nodes", SeqlockBoundary * 256)
    ]

class WaveSharedWriter:
    def __init__(self, filepath: str = None, max_nodes: int = 256):
        if filepath is None:
            filepath = os.path.join(os.path.expanduser("~"), "wave_shm.dat")
        self.filepath = filepath
        self.size = ctypes.sizeof(TrajectorySharedState)
        self._fd = None
        self._mmap = None
        self.state = None
        self._init_mmap()

    def _init_mmap(self):
        self._fd = os.open(self.filepath, os.O_CREAT | os.O_RDWR)
        os.ftruncate(self._fd, self.size)
        self._mmap = mmap.mmap(self._fd, self.size, mmap.MAP_SHARED, mmap.PROT_WRITE | mmap.PROT_READ)
        self.state = TrajectorySharedState.from_buffer(self._mmap)

    def set_active_nodes(self, count: int):
        self.state.active_node_count = count

    def write_boundary(self, node_id: int, boundary_data: Dict[str, Union[int, float]]):
        if node_id < 0 or node_id >= 256:
            raise ValueError("Node ID out of bounds")
        target = self.state.nodes[node_id]
        seq = target.sequence_counter
        if seq % 2 != 0:
            seq += 1
        target.sequence_counter = seq + 1
        target.data.boundary_id = int(boundary_data.get("boundary_id", 0))
        target.data.min_phase_amplitude = float(boundary_data.get("min_phase_amplitude", 0.0))
        target.data.max_phase_amplitude = float(boundary_data.get("max_phase_amplitude", 0.0))
        target.data.resonance_threshold = float(boundary_data.get("resonance_threshold", 0.0))
        target.data.active_constraint_flags = int(boundary_data.get("active_constraint_flags", 0))
        target.sequence_counter = seq + 2

    def close(self):
        # 1. Delete the ctypes struct to release the buffer lock
        if self.state is not None:
            del self.state
            self.state = None
        # 2. Close the mmap
        if self._mmap is not None:
            self._mmap.close()
            self._mmap = None
        # 3. Close the file descriptor
        if self._fd is not None:
            try:
                os.close(self._fd)
            except OSError:
                pass
            self._fd = None

    def __del__(self):
        self.close()
