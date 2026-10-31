// A small QR Code encoder for the share link: byte mode, error correction
// level M, versions 1 to 10 (up to 213 bytes). Follows ISO/IEC 18004 in the
// same steps as Project Nayuki's reference encoder; no external code.

const ECC_PER_BLOCK = [0, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26];
const BLOCKS = [0, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5];
const MAX_VERSION = 10;
const FORMAT_LEVEL_M = 0;

// Modules that carry data or error correction, after all function patterns.
function rawDataModules(version) {
  let result = (16 * version + 128) * version + 64;
  if (version >= 2) {
    const align = Math.floor(version / 7) + 2;
    result -= (25 * align - 10) * align - 55;
    if (version >= 7) result -= 36;
  }
  return result;
}

function dataCodewords(version) {
  return Math.floor(rawDataModules(version) / 8) - ECC_PER_BLOCK[version] * BLOCKS[version];
}

function alignmentPositions(version) {
  if (version === 1) return [];
  const count = Math.floor(version / 7) + 2;
  const step = Math.ceil((version * 4 + 4) / (count * 2 - 2)) * 2;
  const positions = [6];
  for (let pos = version * 4 + 10; positions.length < count; pos -= step) positions.splice(1, 0, pos);
  return positions;
}

// --- Reed-Solomon over GF(256) with the QR polynomial 0x11D ---

function gfMultiply(x, y) {
  let z = 0;
  for (let i = 7; i >= 0; i--) {
    z = (z << 1) ^ ((z >>> 7) * 0x11d);
    z ^= ((y >>> i) & 1) * x;
  }
  return z;
}

function rsDivisor(degree) {
  const result = new Array(degree).fill(0);
  result[degree - 1] = 1;
  let root = 1;
  for (let i = 0; i < degree; i++) {
    for (let j = 0; j < degree; j++) {
      result[j] = gfMultiply(result[j], root);
      if (j + 1 < degree) result[j] ^= result[j + 1];
    }
    root = gfMultiply(root, 2);
  }
  return result;
}

function rsRemainder(data, divisor) {
  const result = new Array(divisor.length).fill(0);
  for (const byte of data) {
    const factor = byte ^ result.shift();
    result.push(0);
    divisor.forEach((coefficient, i) => (result[i] ^= gfMultiply(coefficient, factor)));
  }
  return result;
}

// --- Data codewords ---

function encodeData(bytes, version) {
  const bits = [];
  const push = (value, length) => {
    for (let i = length - 1; i >= 0; i--) bits.push((value >>> i) & 1);
  };
  push(0b0100, 4); // byte mode
  push(bytes.length, version <= 9 ? 8 : 16);
  for (const byte of bytes) push(byte, 8);

  const capacity = dataCodewords(version) * 8;
  push(0, Math.min(4, capacity - bits.length)); // terminator
  push(0, (8 - (bits.length % 8)) % 8);
  for (let pad = 0xec; bits.length < capacity; pad ^= 0xec ^ 0x11) push(pad, 8);

  const codewords = [];
  for (let i = 0; i < bits.length; i += 8) codewords.push(parseInt(bits.slice(i, i + 8).join(''), 2));
  return codewords;
}

// Splits the data into blocks, adds error correction and interleaves them.
function addErrorCorrection(data, version) {
  const blockCount = BLOCKS[version];
  const eccLength = ECC_PER_BLOCK[version];
  const total = Math.floor(rawDataModules(version) / 8);
  const shortBlocks = blockCount - (total % blockCount);
  const shortLength = Math.floor(total / blockCount);
  const divisor = rsDivisor(eccLength);

  const blocks = [];
  for (let i = 0, k = 0; i < blockCount; i++) {
    const block = data.slice(k, k + shortLength - eccLength + (i < shortBlocks ? 0 : 1));
    k += block.length;
    const ecc = rsRemainder(block, divisor);
    if (i < shortBlocks) block.push(0); // placeholder, skipped below
    blocks.push(block.concat(ecc));
  }
  const result = [];
  for (let i = 0; i < blocks[0].length; i++) {
    blocks.forEach((block, j) => {
      if (i !== shortLength - eccLength || j >= shortBlocks) result.push(block[i]);
    });
  }
  return result;
}

// --- The module grid ---

class Grid {
  constructor(version) {
    this.size = version * 4 + 17;
    this.dark = Array.from({ length: this.size }, () => new Array(this.size).fill(false));
    this.reserved = Array.from({ length: this.size }, () => new Array(this.size).fill(false));
  }

  setFunction(x, y, dark) {
    this.dark[y][x] = dark;
    this.reserved[y][x] = true;
  }
}

function drawFinder(grid, cx, cy) {
  for (let dy = -4; dy <= 4; dy++) {
    for (let dx = -4; dx <= 4; dx++) {
      const x = cx + dx;
      const y = cy + dy;
      const ring = Math.max(Math.abs(dx), Math.abs(dy));
      if (x >= 0 && x < grid.size && y >= 0 && y < grid.size) grid.setFunction(x, y, ring !== 2 && ring !== 4);
    }
  }
}

function drawAlignment(grid, cx, cy) {
  for (let dy = -2; dy <= 2; dy++)
    for (let dx = -2; dx <= 2; dx++) grid.setFunction(cx + dx, cy + dy, Math.max(Math.abs(dx), Math.abs(dy)) !== 1);
}

function drawFormatBits(grid, mask) {
  const data = (FORMAT_LEVEL_M << 3) | mask;
  let rem = data;
  for (let i = 0; i < 10; i++) rem = (rem << 1) ^ ((rem >>> 9) * 0x537);
  const bits = ((data << 10) | rem) ^ 0x5412;
  const bit = (i) => ((bits >>> i) & 1) === 1;
  const size = grid.size;

  for (let i = 0; i <= 5; i++) grid.setFunction(8, i, bit(i));
  grid.setFunction(8, 7, bit(6));
  grid.setFunction(8, 8, bit(7));
  grid.setFunction(7, 8, bit(8));
  for (let i = 9; i < 15; i++) grid.setFunction(14 - i, 8, bit(i));

  for (let i = 0; i < 8; i++) grid.setFunction(size - 1 - i, 8, bit(i));
  for (let i = 8; i < 15; i++) grid.setFunction(8, size - 15 + i, bit(i));
  grid.setFunction(8, size - 8, true); // the dark module
}

function drawVersionBits(grid, version) {
  if (version < 7) return;
  let rem = version;
  for (let i = 0; i < 12; i++) rem = (rem << 1) ^ ((rem >>> 11) * 0x1f25);
  const bits = (version << 12) | rem;
  for (let i = 0; i < 18; i++) {
    const dark = ((bits >>> i) & 1) === 1;
    const a = grid.size - 11 + (i % 3);
    const b = Math.floor(i / 3);
    grid.setFunction(a, b, dark);
    grid.setFunction(b, a, dark);
  }
}

function drawFunctionPatterns(grid, version) {
  for (let i = 0; i < grid.size; i++) {
    grid.setFunction(6, i, i % 2 === 0);
    grid.setFunction(i, 6, i % 2 === 0);
  }
  drawFinder(grid, 3, 3);
  drawFinder(grid, grid.size - 4, 3);
  drawFinder(grid, 3, grid.size - 4);
  const positions = alignmentPositions(version);
  const last = positions.length - 1;
  positions.forEach((x, i) => {
    positions.forEach((y, j) => {
      const nearFinder = (i === 0 && j === 0) || (i === 0 && j === last) || (i === last && j === 0);
      if (!nearFinder) drawAlignment(grid, x, y);
    });
  });
  drawFormatBits(grid, 0); // reserves the area; redrawn with the real mask
  drawVersionBits(grid, version);
}

// Places the codewords in the two-column zigzag from the bottom right.
function drawCodewords(grid, codewords) {
  let i = 0;
  for (let right = grid.size - 1; right >= 1; right -= 2) {
    if (right === 6) right = 5; // skip the vertical timing pattern
    for (let vert = 0; vert < grid.size; vert++) {
      for (let j = 0; j < 2; j++) {
        const x = right - j;
        const upward = ((right + 1) & 2) === 0;
        const y = upward ? grid.size - 1 - vert : vert;
        if (grid.reserved[y][x] || i >= codewords.length * 8) continue;
        grid.dark[y][x] = ((codewords[i >>> 3] >>> (7 - (i & 7))) & 1) === 1;
        i++;
      }
    }
  }
}

const MASKS = [
  (x, y) => (x + y) % 2 === 0,
  (x, y) => y % 2 === 0,
  (x) => x % 3 === 0,
  (x, y) => (x + y) % 3 === 0,
  (x, y) => (Math.floor(x / 3) + Math.floor(y / 2)) % 2 === 0,
  (x, y) => ((x * y) % 2) + ((x * y) % 3) === 0,
  (x, y) => (((x * y) % 2) + ((x * y) % 3)) % 2 === 0,
  (x, y) => (((x + y) % 2) + ((x * y) % 3)) % 2 === 0,
];

// XOR is its own inverse, so applying a mask twice removes it again.
function applyMask(grid, mask) {
  for (let y = 0; y < grid.size; y++)
    for (let x = 0; x < grid.size; x++) if (!grid.reserved[y][x] && MASKS[mask](x, y)) grid.dark[y][x] = !grid.dark[y][x];
}

// The four penalty rules of the standard; the lowest score picks the mask.
function penalty(grid) {
  const size = grid.size;
  const at = (x, y) => grid.dark[y][x];
  let score = 0;

  for (const line of [(i, j) => at(j, i), (i, j) => at(i, j)]) {
    for (let i = 0; i < size; i++) {
      let run = 1;
      for (let j = 1; j < size; j++) {
        if (line(i, j) !== line(i, j - 1)) run = 1;
        else if (++run === 5) score += 3;
        else if (run > 5) score += 1;
      }
    }
  }

  let dark = 0;
  for (let y = 0; y < size; y++) {
    let row = 0;
    let column = 0;
    for (let x = 0; x < size; x++) {
      const here = at(x, y);
      if (x > 0 && y > 0 && here === at(x - 1, y - 1) && here === at(x, y - 1) && here === at(x - 1, y)) score += 3;
      // 1:1:3:1:1 finder look-alikes with four light modules on one side
      row = ((row << 1) & 0x7ff) | (here ? 1 : 0);
      column = ((column << 1) & 0x7ff) | (at(y, x) ? 1 : 0);
      if (x >= 10) {
        if (row === 0x05d || row === 0x5d0) score += 40;
        if (column === 0x05d || column === 0x5d0) score += 40;
      }
      if (here) dark++;
    }
  }

  const total = size * size;
  for (let k = 0; dark * 20 < (9 - k) * total || dark * 20 > (11 + k) * total; k++) score += 10;
  return score;
}

// The QR Code for the text as rows of booleans (true = dark), without quiet zone.
// Throws if the text does not fit into version 10.
export function qrModules(text) {
  const bytes = Array.from(new TextEncoder().encode(text));
  let version = 1;
  while (version <= MAX_VERSION && dataCodewords(version) < bytes.length + (version <= 9 ? 2 : 3)) version++;
  if (version > MAX_VERSION) throw new Error('QR text too long');

  const grid = new Grid(version);
  drawFunctionPatterns(grid, version);
  drawCodewords(grid, addErrorCorrection(encodeData(bytes, version), version));

  let best = 0;
  let bestScore = Infinity;
  for (let mask = 0; mask < 8; mask++) {
    applyMask(grid, mask);
    drawFormatBits(grid, mask);
    const score = penalty(grid);
    if (score < bestScore) {
      best = mask;
      bestScore = score;
    }
    applyMask(grid, mask);
  }
  applyMask(grid, best);
  drawFormatBits(grid, best);
  return grid.dark;
}

// The QR Code as an SVG element: dark modules on a light square, four modules of quiet zone.
export function qrSvg(text, label) {
  const modules = qrModules(text);
  const size = modules.length + 8;
  const ns = 'http://www.w3.org/2000/svg';
  const svg = document.createElementNS(ns, 'svg');
  svg.setAttribute('viewBox', `0 0 ${size} ${size}`);
  svg.setAttribute('role', 'img');
  svg.setAttribute('aria-label', label);
  svg.setAttribute('shape-rendering', 'crispEdges');
  const background = document.createElementNS(ns, 'rect');
  background.setAttribute('width', size);
  background.setAttribute('height', size);
  background.setAttribute('class', 'qr-light');
  let path = '';
  modules.forEach((row, y) => row.forEach((dark, x) => dark && (path += `M${x + 4} ${y + 4}h1v1h-1z`)));
  const marks = document.createElementNS(ns, 'path');
  marks.setAttribute('d', path);
  marks.setAttribute('class', 'qr-dark');
  svg.append(background, marks);
  return svg;
}
