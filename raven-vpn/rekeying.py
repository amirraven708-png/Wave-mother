"""
Raven VPN - Rekeying Mechanism
Reference implementation (crypto primitives are placeholders - see README).

Key hierarchy:
    Root Key (from Kyber, epoch ~24h)
      -> Chain Key (rolling, ~1h, local HKDF only, no network)
        -> Session Keys (TX/RX, ephemeral, replaced every chain rekey)

This file contains ONLY executable code. Design rationale, diagrams,
and security proofs live in docs/REKEY_SPEC.md, not here.
"""

import time
import hmac
import hashlib
import secrets
from dataclasses import dataclass


# ---------------------------------------------------------------------------
# HKDF (RFC 5869) - production: swap for `cryptography.hazmat...HKDF`
# ---------------------------------------------------------------------------

def hkdf(salt: bytes, ikm: bytes, info: bytes, length: int) -> bytes:
    if not salt:
        salt = b"\x00" * 32

    prk = hmac.new(salt, ikm, hashlib.sha256).digest()

    output = b""
    counter = 1
    while len(output) < length:
        output += hmac.new(
            prk, output[-32:] + info + bytes([counter]), hashlib.sha256
        ).digest()
        counter += 1

    return output[:length]


# ---------------------------------------------------------------------------
# Session keys
# ---------------------------------------------------------------------------

@dataclass
class SessionKeys:
    tx_key: bytes
    rx_key: bytes

    def __post_init__(self):
        assert len(self.tx_key) == 32
        assert len(self.rx_key) == 32


# ---------------------------------------------------------------------------
# Replay window (WireGuard-style 64-slot bitmap)
# ---------------------------------------------------------------------------

class ReplayWindow:
    def __init__(self, size: int = 64):
        self.highest_seen = -1  # -1 => nothing received yet
        self.bitmap = 0
        self.window_size = size

    def check_and_update(self, nonce: int) -> bool:
        if self.highest_seen == -1:
            self.highest_seen = nonce
            self.bitmap = 1
            return True

        if nonce > self.highest_seen:
            delta = nonce - self.highest_seen
            self.bitmap = (self.bitmap << delta) | 1 if delta < self.window_size else 1
            self.highest_seen = nonce
            return True

        if nonce >= (self.highest_seen - self.window_size + 1):
            bit = self.highest_seen - nonce
            mask = 1 << bit
            if self.bitmap & mask:
                return False  # replay
            self.bitmap |= mask
            return True

        return False  # too old, outside window

    def reset(self):
        self.highest_seen = -1
        self.bitmap = 0


# ---------------------------------------------------------------------------
# Rekeying state machine
# ---------------------------------------------------------------------------

class RekeyingState:
    def __init__(self, root_key: bytes,
                 rekey_interval: int = 3600,
                 epoch_lifetime: int = 86400):
        self.root_key = root_key
        self.epoch_start = time.time()

        self.chain_counter = 0
        self.chain_key = self._derive_initial_chain_key()
        self.session_keys = self._derive_session_keys()

        self.tx_nonce = 0
        self.rx_window = ReplayWindow()

        self.last_rekey = time.time()
        self.rekey_interval = rekey_interval
        self.epoch_lifetime = epoch_lifetime

    # -- derivation --------------------------------------------------------

    def _derive_initial_chain_key(self) -> bytes:
        return hkdf(
            salt=self.root_key,
            ikm=b"\x01",
            info=b"raven-chain-init-v1",
            length=32,
        )

    def _derive_chain_key(self, prev_chain_key: bytes, counter: int) -> bytes:
        info = b"raven-chain-v1" + counter.to_bytes(8, "big")
        return hkdf(salt=prev_chain_key, ikm=b"\x01", info=info, length=32)

    def _derive_session_keys(self) -> SessionKeys:
        km = hkdf(
            salt=self.chain_key,
            ikm=b"\x02",
            info=b"raven-session-v1",
            length=64,
        )
        return SessionKeys(tx_key=km[:32], rx_key=km[32:64])

    # -- timers --------------------------------------------------------------

    def should_rekey(self) -> bool:
        return (time.time() - self.last_rekey) >= self.rekey_interval

    def should_epoch_rekey(self) -> bool:
        return (time.time() - self.epoch_start) >= self.epoch_lifetime

    # -- rekey ---------------------------------------------------------------

    def perform_chain_rekey(self):
        new_counter = self.chain_counter + 1
        new_chain_key = self._derive_chain_key(self.chain_key, new_counter)

        # forward secrecy: drop references to old key material
        del self.chain_key
        del self.session_keys

        self.chain_counter = new_counter
        self.chain_key = new_chain_key
        self.session_keys = self._derive_session_keys()

        self.tx_nonce = 0
        self.rx_window.reset()
        self.last_rekey = time.time()

    # -- nonces ---------------------------------------------------------------

    def get_tx_nonce(self) -> bytes:
        nonce = self.tx_nonce
        self.tx_nonce += 1

        if self.tx_nonce >= 2**96:
            self.perform_chain_rekey()

        return nonce.to_bytes(12, "big")

    def check_rx_nonce(self, nonce: int) -> bool:
        return self.rx_window.check_and_update(nonce)


# ---------------------------------------------------------------------------
# Self-test
# ---------------------------------------------------------------------------

def test():
    print("╔══════════════════════════════════════════╗")
    print("║   RAVEN VPN REKEYING MECHANISM - TEST    ║")
    print("╚══════════════════════════════════════════╝\n")

    root_key = secrets.token_bytes(32)
    s = RekeyingState(root_key)

    print("Test 1: Initial State")
    assert len(s.session_keys.tx_key) == 32
    assert len(s.session_keys.rx_key) == 32
    assert s.chain_counter == 0
    print("   ✅ Pass\n")

    print("Test 2: Chain Rekey")
    old_tx = s.session_keys.tx_key
    s.perform_chain_rekey()
    assert s.session_keys.tx_key != old_tx
    assert s.chain_counter == 1
    assert s.tx_nonce == 0
    print(f"🔄 Chain rekey complete (counter: {s.chain_counter})")
    print("   ✅ Pass\n")

    print("Test 3: Replay Protection (SAME session)")
    assert s.check_rx_nonce(1) == True
    assert s.check_rx_nonce(1) == False   # replay
    assert s.check_rx_nonce(2) == True
    print("   ✅ Pass\n")

    print("Test 4: Out-of-Order Acceptance")
    assert s.check_rx_nonce(10) == True
    assert s.check_rx_nonce(5) == True
    assert s.check_rx_nonce(5) == False
    print("   ✅ Pass\n")

    print("Test 5: Nonce Overflow Triggers Rekey")
    s.tx_nonce = 2**96 - 1
    s.get_tx_nonce()
    assert s.tx_nonce == 0  # rekey reset it
    print("   ✅ Pass\n")

    print("╔══════════════════════════════════════════╗")
    print("║           ALL TESTS PASSED ✅            ║")
    print("╚══════════════════════════════════════════╝")


if __name__ == "__main__":
    test()
