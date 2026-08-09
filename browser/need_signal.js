class NeedSignal {
    constructor(rhythm, requested, available, phase) {
        this.id = crypto.randomUUID();
        this.rhythm = rhythm;
        this.requested = requested;
        this.remaining = Math.max(0, requested - available);
        this.phase = phase;
        this.createdAt = Date.now();
        this.active = this.remaining > 0;
        this.suppliers = new Map();
    }

    supply(nodeId, amount) {
        if (!this.active || amount <= 0) return false;
        const accepted = Math.min(amount, this.remaining);
        this.remaining -= accepted;
        this.suppliers.set(nodeId, (this.suppliers.get(nodeId) || 0) + accepted);
        if (this.remaining === 0) this.active = false;
        return true;
    }

    isResolved() { return !this.active; }
}
