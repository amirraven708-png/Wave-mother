#!/usr/bin/env python3
"""Raven VPN - Rekeying Mechanism"""

import time, hmac, hashlib, secrets
from dataclasses import dataclass

def hkdf(salt, ikm, info, length):
    if not salt: salt = b'\x00' * 32
    prk = hmac.new(salt, ikm, hashlib.sha256).digest()
    out, c = b'', 1
    while len(out) < length:
        out += hmac.new(prk, out[-32:] + info + bytes([c]), hashlib.sha256).digest()
        c += 1
    return out[:length]

@dataclass
class SessionKeys:
    tx_key: bytes; rx_key: bytes

class ReplayWindow:
    def __init__(self, size=64):
        self.highest, self.bitmap, self.size = 0, 0, size
    def check(self, n):
        if n > self.highest:
            d = n - self.highest
            self.bitmap = (self.bitmap << d) if d < self.size else 0
            self.highest = n; return True
        if n >= self.highest - self.size + 1:
            b = 1 << (self.highest - n)
            if self.bitmap & b: return False
            self.bitmap |= b; return True
        return False
    def reset(self): self.highest = self.bitmap = 0

class RekeyingState:
    def __init__(self, root_key):
        self.root_key = root_key
        self.chain_counter = 0
        self.chain_key = hkdf(root_key, b'\x01', b'raven-chain-init-v1', 32)
        self.session_keys = self._derive_session_keys()
        self.tx_nonce = 0
        self.rx_window = ReplayWindow()

    def _derive_session_keys(self):
        km = hkdf(self.chain_key, b'\x02', b'raven-session-v1', 64)
        return SessionKeys(km[:32], km[32:])

    def _derive_chain_key(self, prev, cnt):
        return hkdf(prev, b'\x01', b'raven-chain-v1' + cnt.to_bytes(8, 'big'), 32)

    def perform_chain_rekey(self):
        self.chain_counter += 1
        self.chain_key = self._derive_chain_key(self.chain_key, self.chain_counter)
        self.session_keys = self._derive_session_keys()
        self.tx_nonce = 0
        self.rx_window.reset()
        print(f"🔄 Chain rekey complete (counter: {self.chain_counter})")

def test():
    print("╔══════════════════════════════════════════╗")
    print("║   RAVEN VPN REKEYING MECHANISM - TEST   ║")
    print("╚══════════════════════════════════════════╝\n")

    s = RekeyingState(secrets.token_bytes(32))

    print("Test 1: Initial State")
    assert s.chain_counter == 0; print("   ✅ Pass\n")

    print("Test 2: Chain Rekey")
    old_tx = s.session_keys.tx_key
    s.perform_chain_rekey()
    assert old_tx != s.session_keys.tx_key
    assert s.chain_counter == 1
    assert s.tx_nonce == 0
    print("   ✅ Pass\n")

    print("Test 3: Replay Protection (SAME session)")
    assert s.check_rx_nonce(1) == True
    assert s.check_rx_nonce(1) == False   # replay → reject
    assert s.check_rx_nonce(2) == True
    assert s.check_rx_nonce(0) == False   # too old
    print("   ✅ Pass\n")

    print("Test 4: Out-of-Order Acceptance")
    assert s.check_rx_nonce(10) == True
    assert s.check_rx_nonce(5) == True    # in window, not seen
    assert s.check_rx_nonce(5) == False   # now seen → replay
    print("   ✅ Pass\n")

    print("Test 5: Nonce Overflow Protection")
    s.tx_nonce = 2**96 - 2
    s.get_tx_nonce()
    assert s.tx_nonce == 1
    print("   ✅ Pass\n")

    print("╔══════════════════════════════════════════╗")
    print("║        ALL TESTS PASSED ✅              ║")
    print("╚══════════════════════════════════════════╝")

if __name__ == "__main__":
    test()
