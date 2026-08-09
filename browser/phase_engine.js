class PhaseEngine {
    constructor() {
        this.phase = Math.random() * Math.PI * 2;
        this.frequency = 1.0;
        this.coupling = 0.05;
    }

    step(dt = 0.1) {
        this.phase += this.frequency * dt;
        this.phase %= Math.PI * 2;
    }

    couple(neighborPhase) {
        const delta = neighborPhase - this.phase;
        this.phase += this.coupling * Math.sin(delta);
        this.phase %= Math.PI * 2;
    }

    degrees() { return this.phase * 180 / Math.PI; }
}
