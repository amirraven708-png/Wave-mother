const crypto = require('crypto');

class GhostMirror {
  constructor(seed) {
    this.seed = seed;
  }

  generateToken(route, sessionId = '', timeSlot = null) {
    const ts = timeSlot || Math.floor(Date.now() / 1000);
    const base = `${this.seed}:${route}:${ts}:${sessionId}`;
    const hash = crypto.createHash('sha256').update(base).digest('hex');
    return hash.substring(0, 12);
  }

  verifyToken(token, route, sessionId = '') {
    const now = Math.floor(Date.now() / 1000);
    for (let offset = -2; offset <= 2; offset++) {
      const candidate = this.generateToken(route, sessionId, now + offset);
      if (candidate === token) {
        return true;
      }
    }
    return false;
  }
}

module.exports = GhostMirror;
