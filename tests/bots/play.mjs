// Eight bots play a whole match through Caddy: a reload storm in round 3 and a
// server restart in the middle of round 5. Needs Node 22+ (built-in WebSocket).
//
//   node tests/bots/play.mjs http://localhost:8080 "docker compose -f deploy/compose.yml restart server"
//
// Without the second argument the restart step is skipped.
import { execSync } from "node:child_process";

const base = process.argv[2] ?? "http://localhost:8080";
const restartCommand = process.argv[3] ?? "";
const wsBase = base.replace(/^http/, "ws");
const names = ["Ada", "Bert", "Cyra", "Dora", "Emil", "Fritz", "Gerd", "Hanna"];

function fail(message) {
  console.error(`FAIL ${message}`);
  process.exit(1);
}

// Waits until check() returns something truthy, or fails with the description.
async function until(description, check, timeoutMs = 15000) {
  const end = Date.now() + timeoutMs;
  while (Date.now() < end) {
    const result = check();
    if (result) return result;
    await new Promise((resolve) => setTimeout(resolve, 20));
  }
  fail(`timed out waiting for: ${description}`);
}

// One browser tab. Keeps the newest state and its own ticket.
function connect(code) {
  const tab = { frames: [], state: null, token: null, seat: null, open: false, closed: false };
  tab.ws = new WebSocket(`${wsBase}/ws/${code}`);
  tab.ws.onopen = () => (tab.open = true);
  tab.ws.onclose = () => (tab.closed = true);
  tab.ws.onmessage = (event) => {
    const frame = JSON.parse(event.data);
    tab.frames.push(frame);
    if (frame.type === "welcome") {
      tab.token = frame.token;
      tab.seat = frame.seat;
    }
    if (frame.type === "state") tab.state = frame;
  };
  tab.send = (message) => tab.ws.send(JSON.stringify(message));
  return tab;
}

async function openTab(code) {
  const tab = connect(code);
  await until("socket open", () => tab.open || tab.closed);
  if (!tab.open) fail("socket refused");
  return tab;
}

// A new tab takes the seat over with the old ticket.
async function resume(code, old) {
  const tab = await openTab(code);
  tab.send({ type: "resume", token: old.token });
  await until("welcome after resume", () => tab.token && tab.state);
  if (tab.seat !== old.seat) fail("resume moved to another seat");
  return tab;
}

async function claimHost(code, hostToken) {
  const host = await openTab(code);
  host.send({ type: "host", token: hostToken });
  await until("host accepted", () => host.frames.some((f) => f.type === "host"));
  return host;
}

function checkPrivacy(players) {
  for (const player of players)
    for (const frame of player.frames)
      if (frame.type === "state" && frame.me.seat !== player.seat && frame.me.seat !== 255)
        fail(`${names[player.seat]} received another seat's private state`);
}

const juror = (players) => players.find((p) => p.state.me.isJuror);

async function answerAll(players, except = null) {
  for (const p of players) {
    if (p.state.me.isJuror || p.state.me.submitted || p === except) continue;
    p.send({ type: "submit", token: p.token, revision: p.state.me.revision,
             slots: [...Array(p.state.pick).keys()] });
  }
}

// After a restart the room must be back, not just the process.
async function waitForRoom(code) {
  const end = Date.now() + 90000;
  while (Date.now() < end) {
    try {
      if ((await fetch(`${base}/api/sessions/${code}`)).status === 200) return;
    } catch {
      // Server still down; try again.
    }
    await new Promise((resolve) => setTimeout(resolve, 250));
  }
  fail("room did not come back after the restart");
}

const room = await (await fetch(`${base}/api/sessions`, { method: "POST", body: "{}" })).json();
console.log(`room ${room.code}`);
let host = await claimHost(room.code, room.hostToken);
let players = [];
for (const name of names) {
  const tab = await openTab(room.code);
  tab.send({ type: "join", name });
  players.push(tab);
}
await until("8 seats", () => players.every((p) => p.state && p.state.players.length === 8));
host.send({ type: "settings", settings: { rounds: 8, answerSecs: 0, extensionSecs: 15,
                                          jurorSecs: 0, autoSecs: 0, rando: true } });
host.send({ type: "start" });
await until("match started", () => players.every((p) => p.state.phase === "playing"));

for (let round = 1; round <= 8; ++round) {
  await until(`round ${round} playing`, () =>
    players.every((p) => p.state.phase === "playing" && p.state.round === round));

  if (round === 3) {
    // Reload storm: every phone opens a new tab before the old one is gone.
    const reloaded = [];
    for (const old of players) reloaded.push(await resume(room.code, old));
    await until("old tabs replaced", () => players.every((p) => p.closed));
    players = reloaded;
    console.log("round 3: reload storm, all 8 seats moved to new sockets");
  }

  if (round === 5 && restartCommand) {
    // Half the answers are in when the server goes down.
    const firstHalf = players.filter((p) => !p.state.me.isJuror).slice(0, 3);
    for (const p of firstHalf)
      p.send({ type: "submit", token: p.token, revision: p.state.me.revision,
               slots: [...Array(p.state.pick).keys()] });
    await until("3 answers in", () => players[0].state.submitted >= 3);
    execSync(restartCommand, { stdio: "inherit" });
    await waitForRoom(room.code);
    const back = [];
    for (const old of players) back.push(await resume(room.code, old));
    players = back;
    host = await claimHost(room.code, room.hostToken);
    if (players[0].state.round !== 5 || players[0].state.submitted < 3)
      fail("restart lost the round or its answers");
    console.log("round 5: server restarted, all 8 seats and 3 answers are back");
  }

  await answerAll(players);
  await until(`round ${round} judging`, () => players.every((p) => p.state.phase === "judging"));
  const j = juror(players);
  j.send({ type: "judge", token: j.token, revision: j.state.me.revision, index: 0 });
  await until(`round ${round} reveal`, () => players.every((p) => p.state.phase === "reveal"));
  host.send({ type: "next" });
}

await until("match finished", () => players.every((p) => p.state.phase === "finished"));
checkPrivacy(players);
const points = players[0].state.players.reduce((sum, p) => sum + p.points, 0);
const humanWins = players[0].state.players.length === 8 && points <= 8;
if (!humanWins) fail(`unexpected scores: ${points} points over 8 rounds`);
console.log(`PASS 8 bots, 8 rounds, reload storm, ${restartCommand ? "restart, " : ""}` +
            `${points} points to players, no foreign hand seen`);
for (const tab of [host, ...players]) tab.ws.close();
process.exit(0);
