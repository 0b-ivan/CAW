// Browser-only invite origin. The server cannot infer the host machine's LAN IP
// when the page was opened on localhost. An explicit address avoids bad QR codes.
const key = (code) => `caw.invite.${code}`;

export function isLoopback(hostname) {
  return hostname === 'localhost' || hostname.endsWith('.localhost') ||
    hostname === '[::1]' || /^127\./.test(hostname);
}

export function validInviteOrigin(text) {
  try {
    const url = new URL(text.trim());
    if (!['http:', 'https:'].includes(url.protocol) || !url.hostname ||
        url.username || url.password || url.pathname !== '/' || url.search || url.hash ||
        isLoopback(url.hostname)) return null;
    return url.origin;
  } catch {
    return null;
  }
}

export function savedInviteOrigin(code) {
  try {
    return validInviteOrigin(localStorage.getItem(key(code)) ?? '');
  } catch {
    return null;
  }
}

export function saveInviteOrigin(code, value) {
  const origin = validInviteOrigin(value);
  if (!origin) return false;
  try {
    localStorage.setItem(key(code), origin);
  } catch {
    // Storage may be blocked. The field stays usable in the current tab.
    temporaryOrigins.set(code, origin);
  }
  temporaryOrigins.set(code, origin);
  return true;
}

const temporaryOrigins = new Map();

function preferredOrigin(code) {
  return temporaryOrigins.get(code) || savedInviteOrigin(code);
}

export function inviteAddressRequired(code) {
  return isLoopback(location.hostname) && !preferredOrigin(code);
}

export function sessionUrl(code) {
  return `${preferredOrigin(code) || location.origin}/s/${code}`;
}

export function shareableSessionUrl(code) {
  return inviteAddressRequired(code) ? null : sessionUrl(code);
}

export function configuredInviteOrigin(code) {
  return preferredOrigin(code);
}
