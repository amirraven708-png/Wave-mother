const express = require('express');
const { initVerifier, ghostAuthMiddleware } = require('./middleware');

const app = express();
const PORT = 3000;

// Initial seed (should come from Discovery Engine / Wave noise)
const initialSeed = Math.floor(Math.random() * 1000000).toString();
initVerifier(initialSeed);
console.log(`🔮 Ghost Mirror initialized with seed: ${initialSeed}`);

// Public endpoint to get seed (for trusted clients)
app.get('/ghost-seed', (req, res) => {
  res.json({ seed: initialSeed });
});

// Protected routes
app.use('/secure', ghostAuthMiddleware);

app.get('/secure/data', (req, res) => {
  res.json({ message: 'This is protected data from Wave Mother' });
});

app.get('/', (req, res) => {
  res.send('<h1>🔮 Ghost Mirror is running</h1>');
});

app.listen(PORT, () => {
  console.log(`🌊 Ghost Mirror listening on port ${PORT}`);
});
