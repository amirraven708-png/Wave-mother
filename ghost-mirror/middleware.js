const FabricVerifier = require('./fabric');

let verifierInstance = null;

function initVerifier(seed) {
  verifierInstance = new FabricVerifier(seed);
}

function ghostAuthMiddleware(req, res, next) {
  if (!verifierInstance) {
    return res.status(500).json({ error: 'Ghost Mirror not initialized' });
  }

  const ghostToken = req.headers['x-ghost-token'] || req.query.ghostToken;
  const route = req.originalUrl;
  const sessionId = req.headers['x-session-id'] || '';

  const result = verifierInstance.verify({ route, ghostToken, sessionId });
  if (!result.valid) {
    return res.status(401).json({ error: 'Unauthorized', reason: result.reason });
  }

  next();
}

module.exports = { initVerifier, ghostAuthMiddleware };
