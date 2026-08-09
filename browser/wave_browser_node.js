class WaveBrowserNode {
    constructor(nodeIdHex) {
        this.nodeId = nodeIdHex.toUpperCase();
        this.db = new BubbleDB();
        this.mesh = new WaveMesh(this);
        this.phaseEngine = new PhaseEngine();
        this.computeScheduler = new ComputeScheduler();
        this.activeNeeds = new Map();
        this.capacity = { compute: 100, storage: 100, bandwidth: 100 };
        this.reputation = 70;
    }

    async boot() {
        await this.db.open();
        await this.computeScheduler.init();
        console.log(`[Wave] Node ${this.nodeId} online`);
        return this;
    }

    async resolveWeURI(uri) {
        if (!uri.startsWith("we://")) throw new Error("Invalid Wave URI");
        const rhythm = uri.slice(5).split("/")[0].toUpperCase();
        const local = await this.db.get(rhythm);
        if (local) return local;
        return this.requestFromMesh(rhythm);
    }

    async requestFromMesh(rhythm) {
        const need = new NeedSignal(rhythm, 1, 0, this.phaseEngine.phase);
        this.activeNeeds.set(need.id, need);
        this.mesh.broadcast(createWavePacket(
            WaveMessageType.NEED_SIGNAL,
            this.nodeId,
            rhythm,
            this.phaseEngine.phase,
            { capacity: { requested: 1, available: 0 }, signalId: need.id }
        ));
        return new Promise(resolve => {
            need.resolve = resolve;
            setTimeout(() => {
                if (!need.isResolved()) {
                    this.activeNeeds.delete(need.id);
                    resolve(null);
                }
            }, 5000);
        });
    }

    receiveMessage(message) {
        switch (message.type) {
            case WaveMessageType.NEED_SIGNAL: this.onNeedSignal(message); break;
            case WaveMessageType.WAVE_SUPPLY: this.onWaveSupply(message); break;
            case WaveMessageType.PHASE_BEACON: this.onPhaseBeacon(message); break;
            case WaveMessageType.TRACE_RESPONSE: this.onTraceResponse(message); break;
        }
    }

    onNeedSignal(msg) {
        if (this.capacity.storage >= msg.capacity.requested) {
            const pkt = createWavePacket(WaveMessageType.WAVE_SUPPLY, this.nodeId, msg.wave.rhythm, this.phaseEngine.phase, {
                capacity: { supplied: msg.capacity.requested },
                signalId: msg.signalId
            });
            this.mesh.send(msg.sender.nodeId, pkt);
        }
    }

    onWaveSupply(msg) {
        const need = this.activeNeeds.get(msg.signalId);
        if (need) {
            need.supply(msg.sender.nodeId, msg.capacity.supplied);
            if (need.isResolved()) {
                this.activeNeeds.delete(need.id);
                need.resolve && need.resolve({ rhythm: need.rhythm, resolved: true });
            }
        }
    }

    onPhaseBeacon(msg) {
        if (typeof msg.wave?.phase === "number") this.phaseEngine.couple(msg.wave.phase);
    }

    onTraceResponse(msg) {
        if (msg.trace) this.db.put(msg.trace);
    }
}

// Make available globally for browser
if (typeof window !== 'undefined') {
    window.BubbleDB = BubbleDB;
    window.WaveMessageType = WaveMessageType;
    window.createWavePacket = createWavePacket;
    window.NeedSignal = NeedSignal;
    window.PhaseEngine = PhaseEngine;
    window.WavePeer = WavePeer;
    window.WaveMesh = WaveMesh;
    window.ComputeScheduler = ComputeScheduler;
    window.WaveBrowserNode = WaveBrowserNode;
}
