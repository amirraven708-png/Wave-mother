class WaveMesh {
    constructor(node) {
        this.node = node;
        this.peers = new Map();
    }

    addPeer(peerId, channel) {
        const peer = new WavePeer(peerId, channel);
        channel.onmessage = (event) => {
            try { this.node.receiveMessage(JSON.parse(event.data)); } catch(e) {}
        };
        channel.onclose = () => { this.peers.delete(peerId); };
        this.peers.set(peerId, peer);
    }

    broadcast(message) {
        for (const peer of this.peers.values()) peer.send(message);
    }

    send(peerId, message) {
        const peer = this.peers.get(peerId);
        if (!peer) return false;
        return peer.send(message);
    }
}
