class WavePeer {
    constructor(peerId, channel) {
        this.peerId = peerId;
        this.channel = channel;
    }

    isOpen() {
        return this.channel && this.channel.readyState === "open";
    }

    send(message) {
        if (!this.isOpen()) return false;
        this.channel.send(JSON.stringify(message));
        return true;
    }
}
