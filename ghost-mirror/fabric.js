const GhostMirror = require('./mirror');

class FabricVerifier {
  constructor(discoverySeed) {
    this.mirror = new GhostMirror(discoverySeed);
    this.usedTokens = new Map();  // token -> expiry timestamp
  }

  verify(request) {
    const { route, ghostToken, sessionId } = request;
    if (!route || !ghostToken) {
      return { valid: false, reason: 'Missing route or ghostToken' };
    }

    // Replay protection
    if (this.usedTokens.has(ghostToken)) {
      const expiry = this.usedTokens.get(ghostToken);
      if (Date.now() < expiry) {
        return { valid: false, reason: 'Token already used (replay)' };
      }
    }

    const isValid = this.mirror.verifyToken(ghostToken, route, sessionId || '');
    if (isValid) {
      this.usedTokens.set(ghostToken, Date.now() + 5000); // 5s expiry
      return { valid: true };
    }
    return { valid: false, reason: 'Invalid ghost token' };
  }
}

module.exports = FabricVerifier;
