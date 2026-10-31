// The WebSocket to the server and the secrets this browser keeps per session.
// Storage can be blocked (private mode); the game then works until the tab closes.

function read(key) {
  try {
    return localStorage.getItem(key);
  } catch {
    return null;
  }
}

function write(key, value) {
  try {
    if (value === null) localStorage.removeItem(key);
    else localStorage.setItem(key, value);
  } catch {
    // Without storage a reload loses the seat; the running tab keeps working.
  }
}

const TOKEN = /^[0-9a-f]{32}$/;

// The seat ticket rotates on every resume; the host token never changes.
export const secrets = {
  ticket: (code) => (TOKEN.test(read(`caw.ticket.${code}`) ?? '') ? read(`caw.ticket.${code}`) : null),
  setTicket: (code, ticket) => write(`caw.ticket.${code}`, ticket),
  hostToken: (code) => (TOKEN.test(read(`caw.host.${code}`) ?? '') ? read(`caw.host.${code}`) : null),
  setHostToken: (code, token) => write(`caw.host.${code}`, token),
  // A public label for the statistics, made once per browser. It proves nothing.
  playerId: () => {
    const saved = read('caw.player');
    if (/^[0-9a-f]{16}$/.test(saved ?? '') && saved !== '0000000000000000') return saved;
    const bytes = crypto.getRandomValues(new Uint8Array(8));
    const id = [...bytes].map((b) => b.toString(16).padStart(2, '0')).join('');
    write('caw.player', id);
    return id;
  },
  language: () => read('caw.lang'),
  setLanguage: (code) => write('caw.lang', code),
};

// One socket at a time. The caller decides about reconnecting in onClose.
// `route` is '' for players and the host, '/screen' for a big screen.
export class Connection {
  constructor(code, { onOpen, onFrame, onClose }, route = '') {
    this.code = code;
    this.route = route;
    this.handlers = { onOpen, onFrame, onClose };
    this.socket = null;
  }

  open() {
    const scheme = location.protocol === 'https:' ? 'wss' : 'ws';
    const socket = new WebSocket(`${scheme}://${location.host}/ws/${this.code}${this.route}`);
    this.socket = socket;
    socket.addEventListener('open', () => this.handlers.onOpen());
    socket.addEventListener('message', (event) => {
      let frame;
      try {
        frame = JSON.parse(event.data);
      } catch {
        return;
      }
      if (frame && typeof frame.type === 'string') this.handlers.onFrame(frame);
    });
    socket.addEventListener('close', () => {
      if (this.socket === socket) this.handlers.onClose();
    });
  }

  isOpen() {
    return this.socket?.readyState === WebSocket.OPEN;
  }

  send(message) {
    if (!this.isOpen()) return false;
    this.socket.send(JSON.stringify(message));
    return true;
  }

  // Closes without reporting it to onClose.
  close() {
    const socket = this.socket;
    this.socket = null;
    socket?.close();
  }
}

// 'found', 'missing' or 'offline' for a session code.
export async function lookupSession(code) {
  try {
    const response = await fetch(`/api/sessions/${code}`, { cache: 'no-store' });
    if (response.status === 404) return { status: 'missing' };
    if (!response.ok) return { status: 'offline' };
    return { status: 'found', language: (await response.json()).language };
  } catch {
    return { status: 'offline' };
  }
}
