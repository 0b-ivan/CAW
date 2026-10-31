#!/usr/bin/env node
// Verify actual WSS through the Cloudflare edge without printing credentials.
const base = 'https://caw.obivan.org';
const result = await fetch(base + '/api/sessions', {
  method: 'POST',
  headers: { 'content-type': 'application/json' },
  body: JSON.stringify({ language: 'de' }),
});
if (result.status !== 201) {
  console.error('FAIL: Cannot create a temporary room via Cloudflare HTTPS');
  process.exit(1);
}
const room = await result.json();
if (!/^[A-HJKMNP-Z2-9]{6}$/.test(room.code) ||
    !/^[0-9a-f]{32}$/.test(room.hostToken)) {
  console.error('FAIL: Unexpected room creation response');
  process.exit(1);
}
const url = 'wss://caw.obivan.org/ws/' + room.code;
const socket = new WebSocket(url);
let seenHost = false;
let seenState = false;
const deadline = setTimeout(() => {
  console.error('FAIL: WSS host/state handshake timed out');
  socket.close();
  process.exitCode = 1;
}, 12000);
socket.addEventListener('open', () => {
  socket.send(JSON.stringify({ type: 'host', token: room.hostToken }));
});
socket.addEventListener('message', ({ data }) => {
  try {
    const frame = JSON.parse(String(data));
    if (frame.type === 'host') seenHost = true;
    if (frame.type === 'state') seenState = true;
    if (frame.type === 'error') throw new Error('Host authentication rejected');
    if (seenHost && seenState) {
      clearTimeout(deadline);
      console.log('PASS: Cloudflare HTTPS and WSS host/state handshake');
      socket.close();
    }
  } catch {
    clearTimeout(deadline);
    console.error('FAIL: Invalid or rejected WSS handshake');
    socket.close();
    process.exitCode = 1;
  }
});
socket.addEventListener('error', () => {
  clearTimeout(deadline);
  console.error('FAIL: Cloudflare WSS connection failed');
  process.exitCode = 1;
});
socket.addEventListener('close', () => {
  if (!seenHost || !seenState) {
    clearTimeout(deadline);
    process.exitCode = 1;
  }
});
