// Host status is the only gold accent in CAW. Keep the SVG small and static.
// The visible HOST text is added by the caller, so the color is never the only cue.
export function hostCrown() {
  const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  svg.setAttribute('viewBox', '0 0 24 24');
  svg.setAttribute('class', 'host-crown');
  svg.setAttribute('aria-hidden', 'true');

  const crown = document.createElementNS('http://www.w3.org/2000/svg', 'path');
  crown.setAttribute('d', 'M2 7 6 11 12 3 18 11 22 7 20 18 H4 Z M4 21 H20');
  crown.setAttribute('fill', 'none');
  crown.setAttribute('stroke', 'currentColor');
  crown.setAttribute('stroke-width', '1.9');
  crown.setAttribute('stroke-linecap', 'round');
  crown.setAttribute('stroke-linejoin', 'round');
  svg.append(crown);
  return svg;
}

// Shared by the phone lobby and the TV lobby; host names remain ordinary text.
export function markHost(item, name, hostLabel) {
  item.classList.add('host-player');
  const player = document.createElement('span');
  player.className = 'host-player-name';
  player.textContent = name;
  const label = document.createElement('span');
  label.className = 'host-role-label';
  label.textContent = hostLabel;
  item.replaceChildren(hostCrown(), player, label);
  return item;
}
