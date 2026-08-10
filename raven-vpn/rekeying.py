#!/usr/bin/env python3
"""
Raven VPN - Rekeying Mechanism
Post-quantum key derivation and rotation
"""

import time
import hmac
import hashlib
import secrets
from dataclasses import dataclass

def hkdf(salt: bytes, ikm: bytes, info: bytes, length: int) -> bytes:
    """HKDF-SHA256"""
    if not salt:
        salt = b'\x00' * 32
    prk = hmac.new(salt, ikm, hashlib.sha256).digest()
    output = b''
    counter = 1
    while len(output) < length:
        output += hmac.new(prk, output[-32:] + info + bytes([counter]), 
                          hashlib.sha256).digest()
        counter += 1
    return output[:length]

@dataclass
class SessionKeys:
    tx_key: bytes
    rx_key: bytes

class ReplayWindow:
    def __init__(self, size: int = 64):
        self.highest_seen = 0
        self.bitmap = 0
        self.window_size = size
    
    def check_and_update(self, nonce: int) -> bool:
        if nonce > self.highest_seen:
            delta = nonce - self.highest_seen
            self.bitmap = (self.bitmap << delta) if delta < self.window_size else 0
            self.highest_seen = nonce
            return True
        if nonce >= (self.highest_seen - self.window_size + 1):
            bit_position = self.highest_seen - nonce
            bit_mask = 1 << bit_position
            if self.bitmap & bit_mask:
                return False
            self.bitmap |= bit_mask
            return True
        return False
    
    def reset(self):
        self.highest_seen = 0
        self.bitmap = 0

class RekeyingState:
    def __init__(self, root_key: bytes):
        self.root_key = root_key
        self.epoch_start = time.time()
        self.chain_counter = 0
        self.chain_key = self._derive_initial_chain_key()
        self.session_keys = self._derive_session_keys()
        self.tx_nonce = 0
        self.rx_window = ReplayWindow()
        self.last_rekey = time.time()
        self.rekey_interval = 3600
        self.epoch_lifetime = 86400
    
    def _derive_initial_chain_key(self) -> bytes:
        return hkdf(salt=self.root_key, ikm=b"\x01",
                    info=b"raven-chain-init-v1", length=32)
    
    def _derive_chain_key(self, prev: bytes, counter: int) -> bytes:
        return hkdf(salt=prev, ikm=b"\x01",
                    info=b"raven-chain-v1" + counter.to_bytes(8, 'big'), length=32)
    
    def _derive_session_keys(self) -> SessionKeys:
        km = hkdf(salt=self.chain_key, ikm=b"\x02",
                  info=b"raven-session-v1", length=64)
        return SessionKeys(tx_key=km[:32], rx_key=km[32:])
    
    def should_rekey(self) -> bool:
        return (time.time() - self.last_rekey) >= self.rekey_interval
    
    def perform_chain_rekey(self):
        new_counter = self.chain_counter + 1
        new_chain_key = self._derive_chain_key(self.chain_key, new_counter)
        del self.chain_key, self.session_keys
        self.chain_counter = new_counter
        self.chain_key = new_chain_key
        self.session_keys = self._derive_session_keys()
        self.tx_nonce = 0
        self.rx_window.reset()
        self.last_rekey = time.time()
        print(f"🔄 Chain rekey complete (counter: {new_counter})")
    
    def get_tx_nonce(self) -> bytes:
        nonce = self.tx_nonce
        self.tx_nonce += 1
        if self.tx_nonce >= 2**96:
            self.perform_chain_rekey()
        return nonce.to_bytes(12, 'big')
    
    def check_rx_nonce(self, nonce: int) -> bool:
        return self.rx_window.check_and_update(nonce)

def test_rekey_mechanism():
    print("\n╔═══════════════════════════════════════════════════════════════╗")
    print("║           RAVEN VPN REKEYING MECHANISM - TEST SUITE          ║")
    print("╚═══════════════════════════════════════════════════════════════╝\n")

    root_key = secrets.token_bytes(32)
    state = RekeyingState(root_key)

    print("Test 1: Initial State")
    print(f"   Chain counter: {state.chain_counter}")
    assert state.chain_counter == 0, "Counter should start at 0"
    assert len(state.session_keys.tx_key) == 32
    assert len(state.session_keys.rx_key) == 32
    print("   ✅ Pass\n")

    print("Test 2: Chain Rekey")
    old_tx = state.session_keys.tx_key
    # Do NOT rekey here; test replay with the same session
    pass # no rekey between tests
    assert old_tx != state.session_keys.tx_key, "Keys must change"
    assert state.chain_counter == 1, "Counter must increment"
    assert state.tx_nonce == 0, "Nonce must reset"
    print("   ✅ Pass\n")

    print("Test 3: Replay Protection")
    assert state.check_rx_nonce(1) == True, "Accept first"
    assert state.check_rx_nonce(1) == False, "Reject replay"
    assert state.check_rx_nonce(2) == True, "Accept next"
    assert state.check_rx_nonce(0) == False, "Reject old"
    print("   ✅ Pass\n")

    print("Test 4: Out-of-Order")
    assert state.check_rx_nonce(10) == True
    assert state.check_rx_nonce(5) == True, "Accept in window"
    assert state.check_rx_nonce(5) == False, "Reject replay"
    print("   ✅ Pass\n")

    print("Test 5: Nonce Overflow")
    state.tx_nonce = 2**96 - 2
    state.get_tx_nonce()
    assert state.tx_nonce == 1, "Must reset after overflow"
    print("   ✅ Pass\n")

    print("╔═══════════════════════════════════════════════════════════════╗")
    print("║                   ALL TESTS PASSED ✅                         ║")
    print("╚═══════════════════════════════════════════════════════════════╝\n")

if __name__ == "__main__":
    test_rekey_mechanism()
