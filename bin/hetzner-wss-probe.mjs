#!/usr/bin/env node
// Check public HTTPS and WSS without creating or mutating a game session.
const origin = new URL(process.argv[2] || 'https://caw.obivan.org');
if (!['http:', 'https:'].includes(origin.protocol)) {
  console.error('FAIL: invalid CAW probe URL');
  process.exit(1);
}

try {
  const health = await fetch(new URL('/healthz', origin), {
    signal: AbortSignal.timeout(8000),
  });
  if (health.status !== 200 || (await health.text()).trim() !== 'ok') {
    throw new Error('HTTPS health failed');
  }

  const address = new URL('/ws/probe', origin);
  address.protocol = origin.protocol === 'https:' ? 'wss:' : 'ws:';
  await new Promise((resolve, reject) => {
    const socket = new WebSocket(address);
    let settled = false;
    const finish = (error) => {
      if (settled) return;
      settled = true;
      clearTimeout(deadline);
      if (socket.readyState === WebSocket.OPEN) socket.close();
      if (error) reject(error);
      else resolve();
    };
    const deadline = setTimeout(
      () => finish(new Error('WSS probe timed out')), 12000,
    );
    socket.addEventListener('message', ({ data }) => {
      if (String(data) === '{"type":"probe","ok":true}') finish();
      else finish(new Error('Unexpected WSS probe response'));
    });
    socket.addEventListener('error', () => finish(new Error('WSS probe failed')));
    socket.addEventListener('close', () =>
      finish(new Error('WSS probe closed before response')));
  });
  console.log('PASS: HTTPS and session-free WSS probe');
} catch {
  // Network exceptions can contain URLs or upstream details: keep logs generic.
  console.error('FAIL: CAW public HTTPS/WSS probe');
  process.exitCode = 1;
}
