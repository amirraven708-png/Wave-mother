const WaveMessageType = Object.freeze({
    HELLO: "HELLO",
    PHASE_BEACON: "PHASE_BEACON",
    NEED_SIGNAL: "NEED_SIGNAL",
    WAVE_SUPPLY: "WAVE_SUPPLY",
    TRACE_REQUEST: "TRACE_REQUEST",
    TRACE_RESPONSE: "TRACE_RESPONSE",
    COMPUTE_OFFER: "COMPUTE_OFFER",
    COMPUTE_RESULT: "COMPUTE_RESULT",
    CALIBRATION: "CALIBRATION"
});

function createWavePacket(type, senderNodeId, rhythm, phase, extras = {}) {
    return {
        version: 1,
        type,
        sender: { nodeId: senderNodeId },
        wave: { rhythm, phase },
        ...extras,
        timestamp: Date.now()
    };
}
