// E-Paper Dashboard setup wizard
'use strict';

const $ = (id) => document.getElementById(id);
const steps = 9;
let cur = 0;

const state = {
  wifiOk: false,
  calHref: '',       // chosen CalDAV calendar URL
  calName: '',
  wx: null,          // {name, lat, lon, tz}
  installed: [],     // block ids installed from the store during setup
};

// ---------- device ----------
// Everything hardware-specific comes from GET /api/state — this page states
// no panel size, refresh time, chip or grid of its own, so adding a preset to
// config.h is enough for the UI to describe that device correctly. The values
// below only stand in for the moment before that call returns.
const DEV = {
  panel: 'e-paper panel', panelW: 800, panelH: 480, colors: 3, refreshSec: 25,
  board: '', chip: 'ESP32', cols: 16, rows: 12,
  hostname: 'epaper-dashboard', apIp: '192.168.4.1', ap: 'EPaper-Dashboard',
  setupPin: 0, registry: '',
};

// ---------- providers ----------
// Presets, not a limit: any IMAP server and any CalDAV server work. Unknown
// domains go through "Detect", which probes the conventional host names on
// the device itself. `domains` is only used to preselect an entry.
const MAIL_PROVIDERS = [
  { id: 'auto', name: 'Detect from my address', group: 'Automatic', auto: true,
    hint: 'Press Detect and the device will try the usual IMAP host names for your domain.' },

  { id: 'migadu', name: 'Migadu', group: 'Providers', host: 'imap.migadu.com',
    hint: 'Your normal Migadu mailbox password (or an app password if you created one).' },
  { id: 'fastmail', name: 'Fastmail', group: 'Providers', host: 'imap.fastmail.com',
    domains: ['fastmail.com', 'fastmail.fm', 'sent.com', 'messagingengine.com'],
    hint: 'Use an app password: Settings → Privacy & Security → App passwords.' },
  { id: 'gmail', name: 'Gmail / Google Workspace', group: 'Providers', host: 'imap.gmail.com',
    domains: ['gmail.com', 'googlemail.com'],
    hint: 'Requires an app password (Google Account → Security → 2-Step Verification → App passwords), and IMAP enabled in Gmail settings.' },
  { id: 'icloud', name: 'iCloud Mail', group: 'Providers', host: 'imap.mail.me.com',
    domains: ['icloud.com', 'me.com', 'mac.com'],
    hint: 'Requires an app-specific password (appleid.apple.com → Sign-In and Security). Username is your full iCloud address.' },
  { id: 'mailboxorg', name: 'mailbox.org', group: 'Providers', host: 'imap.mailbox.org',
    domains: ['mailbox.org'],
    hint: 'Your normal mailbox.org password (or an app password if you use 2FA).' },
  { id: 'posteo', name: 'Posteo', group: 'Providers', host: 'posteo.de',
    domains: ['posteo.de', 'posteo.net'],
    hint: 'Your normal Posteo password.' },
  { id: 'zoho', name: 'Zoho Mail', group: 'Providers', host: 'imap.zoho.com',
    domains: ['zoho.com', 'zohomail.com'],
    hint: 'Use an app password. On a regional account the host is imap.zoho.eu / .in / .com.au — adjust it under Advanced.' },
  { id: 'yahoo', name: 'Yahoo Mail', group: 'Providers', host: 'imap.mail.yahoo.com',
    domains: ['yahoo.com', 'yahoo.co.uk', 'yahoo.de', 'ymail.com', 'rocketmail.com'],
    hint: 'Requires an app password (Account Security → Generate app password).' },
  { id: 'aol', name: 'AOL Mail', group: 'Providers', host: 'imap.aol.com',
    domains: ['aol.com'],
    hint: 'Requires an app password.' },
  { id: 'yandex', name: 'Yandex Mail', group: 'Providers', host: 'imap.yandex.com',
    domains: ['yandex.com', 'yandex.ru', 'ya.ru'],
    hint: 'Requires an app password, and IMAP enabled in Yandex mail settings.' },
  { id: 'gmx', name: 'GMX', group: 'Providers', host: 'imap.gmx.net',
    domains: ['gmx.com', 'gmx.net', 'gmx.de', 'gmx.at', 'gmx.ch'],
    hint: 'Enable IMAP access in the GMX web settings first.' },
  { id: 'webde', name: 'WEB.DE', group: 'Providers', host: 'imap.web.de',
    domains: ['web.de'],
    hint: 'Enable IMAP access in the WEB.DE web settings first.' },
  { id: 'purelymail', name: 'Purelymail', group: 'Providers', host: 'imap.purelymail.com',
    domains: ['purelymail.com'],
    hint: 'Your normal Purelymail password.' },
  { id: 'startmail', name: 'StartMail', group: 'Providers', host: 'imap.startmail.com',
    domains: ['startmail.com'],
    hint: 'Requires an app password created in StartMail settings.' },

  { id: 'cpanel', name: 'Self-hosted / cPanel / Plesk', group: 'Self-hosted', host: '',
    hostFromDomain: 'mail.',
    hint: 'Shared hosting usually answers on mail.yourdomain.com. Press Detect if you are not sure.' },
  { id: 'dovecot', name: 'Dovecot / mailcow / Mailu', group: 'Self-hosted', host: '',
    hostFromDomain: 'imap.',
    hint: 'Any standard IMAP server over TLS on port 993.' },

  { id: 'outlook', name: 'Outlook.com / Microsoft 365', group: 'Password login withdrawn',
    domains: ['outlook.com', 'hotmail.com', 'live.com', 'msn.com', 'passport.com'],
    blocked: 'Microsoft has withdrawn password logins for IMAP — only OAuth2 works, which a device like this cannot do. Options: forward the mail you want to see to a mailbox that still allows IMAP, or leave email off and use the calendar via a published ICS link.' },
  { id: 'proton', name: 'Proton Mail', group: 'Password login withdrawn',
    domains: ['proton.me', 'protonmail.com', 'protonmail.ch', 'pm.me'],
    blocked: 'Proton only exposes IMAP through Proton Mail Bridge, which runs on a computer on your LAN and is not reachable with the credentials you have here.' },
  { id: 'tutanota', name: 'Tuta (Tutanota)', group: 'Password login withdrawn',
    domains: ['tutanota.com', 'tuta.com', 'tutamail.com'],
    blocked: 'Tuta does not offer IMAP at all — its mail is only reachable through its own apps.' },

  { id: 'custom', name: 'Other / any IMAP server', group: 'Other', host: '',
    hint: 'Any server speaking IMAP over TLS works. Fill in the host under Advanced, or press Detect.' },
];

const CAL_PROVIDERS = [
  { id: 'auto', name: 'Same as my email provider', group: 'Automatic', follow: true,
    note: 'Uses the CalDAV server that goes with the provider chosen in the email step.' },

  { id: 'migadu', name: 'Migadu', group: 'Providers', dav: 'https://cdav.migadu.com/' },
  { id: 'fastmail', name: 'Fastmail', group: 'Providers', dav: 'https://caldav.fastmail.com/' },
  { id: 'icloud', name: 'iCloud', group: 'Providers', dav: 'https://caldav.icloud.com/',
    note: 'Use the same app-specific password as iCloud Mail.' },
  { id: 'mailboxorg', name: 'mailbox.org', group: 'Providers', dav: 'https://dav.mailbox.org/' },
  { id: 'posteo', name: 'Posteo', group: 'Providers', dav: 'https://posteo.de:8443/' },
  { id: 'zoho', name: 'Zoho Calendar', group: 'Providers', dav: 'https://calendar.zoho.com/caldav/' },
  { id: 'yahoo', name: 'Yahoo Calendar', group: 'Providers', dav: 'https://caldav.calendar.yahoo.com/' },
  { id: 'yandex', name: 'Yandex Calendar', group: 'Providers', dav: 'https://caldav.yandex.ru/' },

  { id: 'nextcloud', name: 'Nextcloud / ownCloud', group: 'Self-hosted',
    dav: 'https://cloud.example.com/remote.php/dav/',
    note: 'Replace the host with your own. An app password from Settings → Security is safer than your login password.' },
  { id: 'baikal', name: 'Baïkal', group: 'Self-hosted', dav: 'https://dav.example.com/dav.php' },
  { id: 'radicale', name: 'Radicale', group: 'Self-hosted', dav: 'https://dav.example.com/' },
  { id: 'synology', name: 'Synology Calendar', group: 'Self-hosted', dav: 'https://nas.example.com:5001/caldav/' },
  { id: 'zimbra', name: 'Zimbra', group: 'Self-hosted', dav: 'https://mail.example.com/dav/' },

  { id: 'google', name: 'Google Calendar', group: 'ICS only', icsOnly: true,
    note: 'Google CalDAV needs OAuth2, which this device cannot do. Use the secret iCal address instead: Calendar settings → your calendar → "Secret address in iCal format".' },
  { id: 'outlookcal', name: 'Outlook.com / Microsoft 365', group: 'ICS only', icsOnly: true,
    note: 'Publish the calendar (Calendar settings → Shared calendars → Publish) and paste the ICS link it gives you.' },

  { id: 'custom', name: 'Other / any CalDAV server', group: 'Other', dav: '',
    note: 'The device follows /.well-known/caldav, so the plain server URL is usually enough.' },
];

const mailProvider = (id) => MAIL_PROVIDERS.find(p => p.id === id) || MAIL_PROVIDERS[0];
const calProvider = (id) => CAL_PROVIDERS.find(p => p.id === id) || CAL_PROVIDERS[0];

// IANA zone -> POSIX TZ (common zones; "custom" always available)
const TZ = [
  ['America/Los_Angeles', 'PST8PDT,M3.2.0,M11.1.0'],
  ['America/Denver',      'MST7MDT,M3.2.0,M11.1.0'],
  ['America/Phoenix',     'MST7'],
  ['America/Chicago',     'CST6CDT,M3.2.0,M11.1.0'],
  ['America/New_York',    'EST5EDT,M3.2.0,M11.1.0'],
  ['America/Anchorage',   'AKST9AKDT,M3.2.0,M11.1.0'],
  ['Pacific/Honolulu',    'HST10'],
  ['America/Toronto',     'EST5EDT,M3.2.0,M11.1.0'],
  ['America/Vancouver',   'PST8PDT,M3.2.0,M11.1.0'],
  ['America/Mexico_City', 'CST6'],
  ['America/Sao_Paulo',   '<-03>3'],
  ['UTC',                 'UTC0'],
  ['Europe/London',       'GMT0BST,M3.5.0/1,M10.5.0'],
  ['Europe/Dublin',       'GMT0IST,M3.5.0/1,M10.5.0'],
  ['Europe/Lisbon',       'WET0WEST,M3.5.0/1,M10.5.0'],
  ['Europe/Paris',        'CET-1CEST,M3.5.0,M10.5.0/3'],
  ['Europe/Berlin',       'CET-1CEST,M3.5.0,M10.5.0/3'],
  ['Europe/Madrid',       'CET-1CEST,M3.5.0,M10.5.0/3'],
  ['Europe/Rome',         'CET-1CEST,M3.5.0,M10.5.0/3'],
  ['Europe/Zurich',       'CET-1CEST,M3.5.0,M10.5.0/3'],
  ['Europe/Amsterdam',    'CET-1CEST,M3.5.0,M10.5.0/3'],
  ['Europe/Stockholm',    'CET-1CEST,M3.5.0,M10.5.0/3'],
  ['Europe/Athens',       'EET-2EEST,M3.5.0/3,M10.5.0/4'],
  ['Europe/Helsinki',     'EET-2EEST,M3.5.0/3,M10.5.0/4'],
  ['Europe/Moscow',       'MSK-3'],
  ['Asia/Dubai',          '<+04>-4'],
  ['Asia/Kolkata',        'IST-5:30'],
  ['Asia/Bangkok',        '<+07>-7'],
  ['Asia/Singapore',      '<+08>-8'],
  ['Asia/Hong_Kong',      'HKT-8'],
  ['Asia/Shanghai',       'CST-8'],
  ['Asia/Tokyo',          'JST-9'],
  ['Asia/Seoul',          'KST-9'],
  ['Australia/Sydney',    'AEST-10AEDT,M10.1.0,M4.1.0/3'],
  ['Australia/Perth',     'AWST-8'],
  ['Pacific/Auckland',    'NZST-12NZDT,M9.5.0,M4.1.0/3'],
];

// ---------- helpers ----------
function alertBox(id, kind, html) {
  $(id).innerHTML = html ? `<div class="alert alert-${kind} py-2 small">${html}</div>` : '';
}
function esc(s) {
  return String(s ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
}
function busy(btnId, on, label) {
  const b = $(btnId);
  if (on) { b.dataset.t = b.innerHTML; b.disabled = true;
            b.innerHTML = `<span class="spinner-border spinner-border-sm me-1"></span>${label || 'Working…'}`; }
  else { b.disabled = false; b.innerHTML = b.dataset.t; }
}
// fetch with retry — the AP can hiccup while the STA joins a router (channel hop)
async function api(path, opts, tries) {
  tries = tries ?? 3;
  for (let i = 0; i < tries; i++) {
    try {
      const r = await fetch(path, opts);
      return await r.json();
    } catch (e) {
      if (i === tries - 1) throw e;
      await new Promise(res => setTimeout(res, 1500));
    }
  }
}
function go(n) {
  document.querySelectorAll('.step').forEach(el => el.classList.add('step-hidden'));
  $(`step-${n}`).classList.remove('step-hidden');
  cur = n;
  $('progress').style.width = `${Math.max(5, Math.round(n / (steps - 1) * 100))}%`;
  if (n === 6) storeLoad(false);
  window.scrollTo(0, 0);
}
// Populate a <select> from a catalogue, grouping by `group`.
function fillProviderSelect(sel, list) {
  sel.innerHTML = '';
  const groups = [];
  for (const p of list) if (!groups.includes(p.group)) groups.push(p.group);
  for (const g of groups) {
    const og = document.createElement('optgroup');
    og.label = g;
    for (const p of list.filter(x => x.group === g)) og.appendChild(new Option(p.name, p.id));
    sel.appendChild(og);
  }
}

// ---------- device description ----------
function applyDevice(d) {
  Object.assign(DEV, d || {});
  COLS = DEV.cols || 16;
  ROWS = DEV.rows || 12;

  const colorWord = DEV.colors >= 3 ? 'black/white/red' : 'black &amp; white';
  $('hdr-dev').innerHTML =
    `${esc(DEV.panel)} · ${DEV.panelW}×${DEV.panelH} · ${colorWord}` +
    (DEV.chip ? ` · ${esc(DEV.chip)}` : '');
  document.querySelectorAll('[data-dev="chip"]').forEach(e => { e.textContent = DEV.chip || 'the ESP32'; });

  $('rf-hint').innerHTML = DEV.colors >= 3
    ? `Each refresh flashes for about ${DEV.refreshSec} s — that is how a three-colour panel works.`
    : `Each refresh takes about ${DEV.refreshSec} s on this panel.`;

  $('ed-hint').innerHTML =
    `Drag tiles to move · drag the ◢ corner to resize · click to select &amp; configure. ` +
    `The grid is the real ${DEV.panelW}×${DEV.panelH} panel, ${COLS}×${ROWS} cells.`;
  $('ed-preview-hint').textContent =
    `Preview saves this canvas, then draws it on the panel — with your real data when the device has it, ` +
    `sample data otherwise (about ${DEV.refreshSec} s of flashing).`;

  const btn = DEV.setupPin === 0 ? 'BOOT' : `the button on GPIO${DEV.setupPin}`;
  $('done-buttons').innerHTML =
    `To change settings later: tap <strong>RST</strong>, then immediately press &amp; hold ` +
    `<strong>${esc(btn)}</strong> for ~2 seconds — the setup network will come back. ` +
    `(Don't hold it <em>while</em> pressing RST: that puts the chip into its flashing mode instead.)`;

  $('footer-note').innerHTML =
    `e-paper dashboard${DEV.fw ? ' v' + esc(DEV.fw) : ''} · ${esc(DEV.board || DEV.chip)} · ` +
    `served by the device at <span class="mono">${esc(location.host)}</span>`;

  if (DEV.registry) {
    $('store-url').value = DEV.registry;
    $('bl-reg-url').value = DEV.registry;
  }
  setPanelAspect(DEV.panelW, DEV.panelH);
}

// ---------- step 1: wifi ----------
async function scanWifi() {
  $('scan-spin').classList.remove('step-hidden');
  $('btn-scan').disabled = true;
  try {
    const nets = await api('/api/scan', null, 2);
    const list = $('wifi-list');
    list.innerHTML = '';
    (nets || []).forEach(n => {
      const a = document.createElement('a');
      a.className = 'list-group-item list-group-item-action wifi-row d-flex justify-content-between';
      a.innerHTML = `<span>${n.enc ? '🔒' : ''} ${esc(n.ssid)}</span><span class="rssi text-secondary">${n.rssi} dBm</span>`;
      a.onclick = () => {
        document.querySelectorAll('.wifi-row').forEach(r => r.classList.remove('active'));
        a.classList.add('active');
        $('wifi-ssid').value = n.ssid;
        $('wifi-pass').focus();
      };
      list.appendChild(a);
    });
    if (!nets || !nets.length) alertBox('wifi-alert', 'warning', 'No networks found — try scanning again.');
  } catch (e) {
    alertBox('wifi-alert', 'danger', 'Scan failed — try again.');
  }
  $('scan-spin').classList.add('step-hidden');
  $('btn-scan').disabled = false;
}

async function joinWifi() {
  const ssid = $('wifi-ssid').value.trim();
  if (!ssid) { alertBox('wifi-alert', 'warning', 'Enter or pick a network name first.'); return; }
  busy('btn-wifi', true, 'Connecting…');
  alertBox('wifi-alert', '', '');
  try {
    await api('/api/wifi', { method: 'POST', body: JSON.stringify({ ssid, pass: $('wifi-pass').value }) }, 5);
    // poll status for up to ~30 s (with generous retries: AP may hop channels)
    for (let i = 0; i < 20; i++) {
      await new Promise(r => setTimeout(r, 1600));
      let st;
      try { st = await api('/api/wifi/status', null, 2); } catch (e) { continue; }
      if (st.state === 'connected') {
        state.wifiOk = true;
        alertBox('wifi-alert', 'success',
          `Connected to <strong>${esc(ssid)}</strong> (IP ${esc(st.ip)}, ${st.rssi} dBm). Internet is now available for the next steps.`);
        busy('btn-wifi', false);
        setTimeout(() => go(2), 900);
        return;
      }
      if (st.state === 'failed') {
        alertBox('wifi-alert', 'danger', `Couldn't join: ${esc(st.reason || 'wrong password or weak signal')}. Check the password and try again.`);
        busy('btn-wifi', false);
        return;
      }
    }
    alertBox('wifi-alert', 'danger', 'Timed out joining the network. Double-check the password (and that it\'s a 2.4 GHz network), then try again.');
  } catch (e) {
    alertBox('wifi-alert', 'danger', 'Lost contact with the device — make sure you\'re still on the setup WiFi, then retry.');
  }
  busy('btn-wifi', false);
}

// ---------- step 2: email ----------
function emailDomain() {
  const u = $('im-user').value.trim().toLowerCase();
  const at = u.indexOf('@');
  return at > 0 ? u.slice(at + 1) : '';
}
function providerChanged() {
  const p = mailProvider($('im-provider').value);
  const dom = emailDomain();

  if (p.blocked) {
    $('im-provider-note').innerHTML = `<span class="text-danger">${esc(p.blocked)}</span>`;
    $('im-pass-hint').textContent = '';
    $('im-host').value = '';
  } else {
    $('im-provider-note').textContent = '';
    $('im-pass-hint').textContent = p.hint || '';
    if (p.host) $('im-host').value = p.host;
    else if (p.hostFromDomain && dom) $('im-host').value = p.hostFromDomain + dom;
    else if (p.auto && !$('im-host').value && dom) $('im-host').value = 'imap.' + dom;
  }
  // Open Advanced when there is nothing sensible to prefill, so the one field
  // that still needs a human is visible rather than hidden behind a summary.
  $('im-adv').open = !!dom && !p.blocked && !$('im-host').value;

  // The calendar step follows the mail provider unless it has been changed.
  const cal = calProvider($('cd-provider').value);
  if (cal.follow) calProviderChanged();
}
function emailChanged() {
  const dom = emailDomain();
  const u = $('im-user').value.trim();
  if (u) $('cd-user').value = u;
  if (!dom) return;
  // Preselect by domain, but never override a choice the user already made.
  if ($('im-provider').dataset.touched !== '1') {
    const hit = MAIL_PROVIDERS.find(p => (p.domains || []).includes(dom));
    if (hit) $('im-provider').value = hit.id;
  }
  providerChanged();
}
async function detectImap() {
  const dom = emailDomain();
  if (!dom) { alertBox('im-alert', 'warning', 'Enter your email address first.'); return; }
  const known = MAIL_PROVIDERS.find(p => (p.domains || []).includes(dom));
  if (known && known.blocked) {
    alertBox('im-alert', 'danger', esc(known.blocked));
    return;
  }
  busy('btn-detect', true, 'Probing…');
  alertBox('im-alert', '', '');
  try {
    const r = await api('/api/imap/detect?domain=' + encodeURIComponent(dom), null, 2);
    if (r.ok) {
      $('im-host').value = r.host;
      $('im-port').value = r.port || 993;
      alertBox('im-alert', 'success',
        `Found an IMAP server at <span class="mono">${esc(r.host)}:${r.port}</span>. ` +
        `Enter your password and press <em>Test email</em>.` +
        (r.banner ? `<div class="mono text-secondary mt-1">${esc(r.banner.slice(0, 90))}</div>` : ''));
    } else {
      alertBox('im-alert', 'warning',
        esc(r.msg || 'no IMAP server found') +
        (r.tried ? `<div class="mono text-secondary mt-1">tried: ${esc(r.tried.join(', '))}</div>` : ''));
      $('im-adv').open = true;
    }
  } catch (e) {
    alertBox('im-alert', 'danger', 'The device didn\'t answer — check you\'re on the setup WiFi and try again.');
  }
  busy('btn-detect', false);
}
function showChanged() {
  $('im-custom-wrap').classList.toggle('step-hidden', $('im-show').value !== 'custom');
}
function imapCfg() {
  return {
    host: $('im-host').value.trim(), port: parseInt($('im-port').value) || 993,
    user: $('im-user').value.trim(), pass: $('im-pass').value,
    folder: $('im-folder').value.trim() || 'INBOX',
    show: $('im-show').value, custom: $('im-custom').value.trim(),
    count: parseInt($('im-count').value),
  };
}
async function testImap() {
  const c = imapCfg();
  if (!c.user || !c.pass) { alertBox('im-alert', 'warning', 'Enter your address and password first (or leave email out and press Next).'); return; }
  if (!c.host) { alertBox('im-alert', 'warning', 'No IMAP host yet — press Detect, or fill it in under Advanced.'); return; }
  busy('btn-imtest', true, 'Logging in…');
  try {
    const r = await api('/api/test/imap', { method: 'POST', body: JSON.stringify(c) }, 2);
    if (r.ok) {
      const newest = r.shown && r.shown.length ? `Newest match: <em>${esc(r.shown[0].subj)}</em> — ${esc(r.shown[0].from)}` : 'No matching messages right now (that\'s fine).';
      alertBox('im-alert', 'success', `Logged in. <strong>${r.unread}</strong> unread in ${esc(c.folder)}. ${newest}`);
    } else {
      alertBox('im-alert', 'danger', `<strong>${esc(r.stage || 'error')}:</strong> ${esc(r.msg || 'unknown error')}`);
    }
  } catch (e) {
    alertBox('im-alert', 'danger', 'The device didn\'t answer — check you\'re on the setup WiFi and try again.');
  }
  busy('btn-imtest', false);
}

// ---------- step 3: calendar ----------
function calMode() { return document.querySelector('input[name=calmode]:checked').value; }
function calModeChanged() {
  const m = calMode();
  $('cal-caldav-wrap').classList.toggle('step-hidden', m !== 'caldav');
  $('cal-ics-wrap').classList.toggle('step-hidden', m !== 'ics');
  alertBox('cal-alert', '', '');
}
function calProviderChanged() {
  const sel = $('cd-provider');
  let p = calProvider(sel.value);
  // "Same as my email provider" resolves through the mail catalogue.
  if (p.follow) {
    const mailId = $('im-provider').value;
    const twin = CAL_PROVIDERS.find(c => c.id === mailId && !c.follow);
    p = twin || calProvider('custom');
  }
  $('cd-provider-note').textContent = p.note || '';
  if (p.icsOnly) {
    $('cal-ics').checked = true;
    calModeChanged();
    $('ics-hint').textContent = p.note || '';
    return;
  }
  if (p.dav) $('cd-base').value = p.dav;
}
async function discover() {
  const base = $('cd-base').value.trim(), user = $('cd-user').value.trim(), pass = $('cd-pass').value;
  if (!base || !user) { alertBox('cal-alert', 'warning', 'Server, username and password are needed for discovery.'); return; }
  busy('btn-discover', true, 'Searching…');
  alertBox('cal-alert', '', '');
  $('cd-list').innerHTML = '';
  try {
    const r = await api('/api/caldav/discover', { method: 'POST', body: JSON.stringify({ base, user, pass }) }, 2);
    if (r.ok && r.calendars && r.calendars.length) {
      r.calendars.forEach((c, i) => {
        const a = document.createElement('a');
        a.className = 'list-group-item list-group-item-action wifi-row';
        a.innerHTML = `📅 ${esc(c.name)} <span class="text-secondary small mono">${esc(c.href)}</span>`;
        a.onclick = () => {
          document.querySelectorAll('#cd-list .wifi-row').forEach(x => x.classList.remove('active'));
          a.classList.add('active');
          state.calHref = c.href; state.calName = c.name;
          alertBox('cal-alert', 'info', `Selected <strong>${esc(c.name)}</strong> — now press <em>Test calendar</em>.`);
        };
        $('cd-list').appendChild(a);
        if (i === 0) a.click();
      });
    } else {
      alertBox('cal-alert', 'danger', `Discovery failed: ${esc(r.msg || 'no calendars found')}. You can paste a full calendar URL into the server field instead.`);
    }
  } catch (e) {
    alertBox('cal-alert', 'danger', 'The device didn\'t answer — try again.');
  }
  busy('btn-discover', false);
}
function calCfg() {
  const m = calMode();
  if (m === 'caldav') return { mode: 'caldav', url: state.calHref || $('cd-base').value.trim(), user: $('cd-user').value.trim(), pass: $('cd-pass').value };
  if (m === 'ics')    return { mode: 'ics', url: $('ics-url').value.trim(), user: '', pass: '' };
  return { mode: 'none', url: '', user: '', pass: '' };
}
async function testCal() {
  const c = calCfg();
  if (c.mode === 'none') { alertBox('cal-alert', 'info', 'Calendar disabled — press Next.'); return; }
  if (!c.url) { alertBox('cal-alert', 'warning', c.mode === 'caldav' ? 'Run "Find my calendars" first (or paste a calendar URL).' : 'Enter the ICS URL first.'); return; }
  busy('btn-caltest', true, 'Fetching…');
  try {
    const r = await api('/api/test/cal', { method: 'POST', body: JSON.stringify(c) }, 2);
    if (r.ok) {
      const sample = r.sample && r.sample.length ? ` Next up: <em>${esc(r.sample[0].title)}</em> (${esc(r.sample[0].when)}).` : '';
      alertBox('cal-alert', 'success', `Calendar OK — <strong>${r.count}</strong> event(s) in the next 48 h.${sample}`);
    } else {
      alertBox('cal-alert', 'danger', esc(r.msg || 'calendar fetch failed'));
    }
  } catch (e) {
    alertBox('cal-alert', 'danger', 'The device didn\'t answer — try again.');
  }
  busy('btn-caltest', false);
}

// ---------- step 4: weather ----------
async function geocode() {
  const q = $('wx-q').value.trim();
  if (!q) return;
  busy('btn-geo', true, '');
  $('wx-list').innerHTML = '';
  try {
    const r = await api('/api/geocode?q=' + encodeURIComponent(q), null, 2);
    if (r.ok && r.results && r.results.length) {
      r.results.forEach(g => {
        const a = document.createElement('a');
        a.className = 'list-group-item list-group-item-action wifi-row';
        a.innerHTML = `📍 ${esc(g.name)}${g.admin1 ? ', ' + esc(g.admin1) : ''} <span class="text-secondary">(${esc(g.country)})</span>`;
        a.onclick = () => {
          state.wx = g;
          $('wx-list').innerHTML = '';
          $('wx-picked').classList.remove('step-hidden');
          $('wx-picked').innerHTML = `Weather location: <strong>${esc(g.name)}${g.admin1 ? ', ' + esc(g.admin1) : ''}</strong> <span class="mono">(${g.lat.toFixed(3)}, ${g.lon.toFixed(3)})</span>`;
          if (g.tz) preselectTz(g.tz);
        };
        $('wx-list').appendChild(a);
      });
    } else {
      alertBox('wx-alert', 'warning', 'No places found — try a different spelling.');
    }
  } catch (e) {
    alertBox('wx-alert', 'danger', 'Lookup failed — is WiFi step done? (The device needs internet for this.)');
  }
  busy('btn-geo', false);
}

// ---------- step 5: clock ----------
function fillTz() {
  const sel = $('ck-tz');
  TZ.forEach(([iana, posix]) => {
    const o = document.createElement('option');
    o.value = posix; o.textContent = iana; o.dataset.iana = iana;
    sel.appendChild(o);
  });
  const o = document.createElement('option');
  o.value = 'custom'; o.textContent = 'Custom POSIX string…';
  sel.appendChild(o);
  sel.onchange = () => $('ck-tz-custom').classList.toggle('step-hidden', sel.value !== 'custom');
  try { preselectTz(Intl.DateTimeFormat().resolvedOptions().timeZone); } catch (e) {}
}
function preselectTz(iana) {
  const sel = $('ck-tz');
  for (const o of sel.options) if (o.dataset.iana === iana) { sel.value = o.value; sel.onchange(); return; }
}
function fillHours() {
  for (const id of ['qt-start', 'qt-end']) {
    for (let h = 0; h < 24; h++) {
      const o = document.createElement('option');
      o.value = h;
      o.textContent = (h % 12 === 0 ? 12 : h % 12) + (h < 12 ? ' AM' : ' PM');
      $(id).appendChild(o);
    }
  }
  $('qt-start').value = 0; $('qt-end').value = 6;
}

// ---------- step 6: store ----------
// The store and the Blocks tab talk to the same two endpoints; this is the
// browsing half of it, shown during setup so a new device arrives configured
// the way its owner wants rather than only with the built-ins.
const store = { blocks: [], cat: 'all', loaded: false };

async function storeLoad(force) {
  if (store.loaded && !force) return;
  store.loaded = true;
  const url = $('store-url').value.trim();
  if (!url) { $('store-list').innerHTML = '<div class="list-group-item text-secondary small">No registry configured.</div>'; return; }
  $('store-list').innerHTML = '<div class="list-group-item text-secondary small">Loading the store…</div>';
  alertBox('store-alert', '', '');
  try {
    const r = await api('/api/registry?url=' + encodeURIComponent(url), null, 2);
    if (!r.ok) {
      store.loaded = false;
      $('store-list').innerHTML = '';
      alertBox('store-alert', 'warning', esc(r.msg || 'could not load the registry') +
        ' — you can skip this step and add blocks later.');
      return;
    }
    store.blocks = r.blocks || [];
    alertBox('store-alert', r.sigOk ? 'success' : 'warning',
      r.sigOk ? `Registry verified — signed by <span class="mono">${esc(r.keyid)}</span>.`
              : 'This registry is <strong>not</strong> signed by a key this device trusts. Its blocks will be refused unless you allow unsigned blocks in the Blocks tab.');
    storeCats();
    storeRender();
  } catch (e) {
    store.loaded = false;
    $('store-list').innerHTML = '';
    alertBox('store-alert', 'danger', 'The device didn\'t answer — skip this step and add blocks later if it persists.');
  }
}
function storeCats() {
  const cats = ['all', ...new Set(store.blocks.map(b => b.category || 'other'))];
  const wrap = $('store-cats');
  wrap.innerHTML = '';
  for (const c of cats) {
    const b = document.createElement('button');
    b.className = 'btn btn-sm ' + (c === store.cat ? 'btn-epaper' : 'btn-outline-secondary');
    b.textContent = c === 'all' ? 'All' : c;
    b.onclick = () => { store.cat = c; storeCats(); storeRender(); };
    wrap.appendChild(b);
  }
}
function storeRender() {
  const L = $('store-list');
  L.innerHTML = '';
  const shown = store.blocks.filter(b => store.cat === 'all' || (b.category || 'other') === store.cat);
  if (!shown.length) {
    L.innerHTML = '<div class="list-group-item text-secondary small">Nothing in this category.</div>';
  }
  for (const b of shown) {
    const done = state.installed.includes(b.id);
    const d = document.createElement('div');
    d.className = 'list-group-item';
    const size = (b.minW && b.minH) ? `<span class="text-secondary small">${b.minW}×${b.minH} cells</span>` : '';
    // The setup hotspot has no route to the internet, so a screenshot often
    // will not load; it is removed rather than left as a broken image.
    const shot = b.screenshot
      ? `<img class="store-shot me-2" src="${esc(b.screenshot)}" alt="" onerror="this.remove()">` : '';
    d.innerHTML =
      `<div class="d-flex justify-content-between align-items-start gap-2">
         <div class="d-flex align-items-start">${shot}
           <div><b>${esc(b.name)}</b> ${size}
             <div class="text-secondary small">${esc(b.description || '')}</div>
             <div class="text-secondary small">${esc(b.author || '')} · v${esc(b.version || '')}</div>
           </div>
         </div>
         <button class="btn btn-sm ${done ? 'btn-success' : 'btn-outline-dark'}"
                 ${done ? 'disabled' : ''} id="store-btn-${esc(b.id)}"
                 onclick="storeInstall('${esc(b.id)}')">${done ? 'Installed' : 'Install'}</button>
       </div>`;
    L.appendChild(d);
  }
  $('store-count').textContent = state.installed.length
    ? `${state.installed.length} block(s) added — arrange them in the Layout tab after setup.` : '';
}
async function storeInstall(id) {
  const b = store.blocks.find(x => x.id === id);
  if (!b) return;
  const btnId = 'store-btn-' + id;
  busy(btnId, true, '…');
  try {
    const r = await api('/api/blocks/install', { method: 'POST', body: JSON.stringify({ url: b.epb }) }, 2);
    busy(btnId, false);
    if (r.ok) {
      if (!state.installed.includes(id)) state.installed.push(id);
      ed.inited = false;               // layout palette must pick the new block up
      alertBox('store-alert', r.sigOk ? 'success' : 'warning',
        `<strong>${esc(b.name)}</strong> installed` +
        (r.sigOk ? ` — signature verified (${esc(r.keyid)}).` : ' — UNSIGNED.'));
      storeRender();
    } else {
      alertBox('store-alert', 'danger', `${esc(b.name)}: ${esc(r.msg || 'install failed')}`);
    }
  } catch (e) {
    busy(btnId, false);
    alertBox('store-alert', 'danger', 'The device didn\'t answer — try again.');
  }
}

// ---------- step 7: review + save ----------
function cfg() {
  const tzSel = $('ck-tz');
  const posix = tzSel.value === 'custom' ? $('ck-tz-custom').value.trim() : tzSel.value;
  const iana = tzSel.value === 'custom' ? 'custom' : tzSel.selectedOptions[0].dataset.iana;
  return {
    wifi: { ssid: $('wifi-ssid').value.trim(), pass: $('wifi-pass').value },
    imap: imapCfg(),
    cal: calCfg(),
    wx: state.wx ? { lat: state.wx.lat, lon: state.wx.lon, place: state.wx.name, unitT: $('wx-unitT').value, unitW: $('wx-unitW').value }
                 : { lat: 0, lon: 0, place: '', unitT: $('wx-unitT').value, unitW: $('wx-unitW').value },
    clock: { h24: $('ck-h24').value === '1', tz: posix || 'UTC0', tzname: iana },
    refresh: { min: parseInt($('rf-min').value), quiet: $('qt-on').checked,
               qs: parseInt($('qt-start').value), qe: parseInt($('qt-end').value) },
  };
}
function buildReview() {
  const c = cfg();
  const row = (k, v, warn) => `<tr><th class="text-secondary fw-normal">${k}</th><td>${v}${warn ? ` <span class="badge text-bg-warning">${warn}</span>` : ''}</td></tr>`;
  $('review').innerHTML =
    row('Device', `${esc(DEV.panel)} <span class="mono">${DEV.panelW}×${DEV.panelH}</span>`) +
    row('WiFi', esc(c.wifi.ssid) || '—', !state.wifiOk && c.wifi.ssid ? 'not tested' : '') +
    row('Email', c.imap.user ? `${esc(c.imap.user)} <span class="mono">(${esc(c.imap.host)})</span>` : 'disabled') +
    row('Calendar', c.cal.mode === 'none' ? 'disabled' : `${c.cal.mode.toUpperCase()}: <span class="mono">${esc((state.calName || c.cal.url).slice(0, 60))}</span>`) +
    row('Weather', c.wx.place ? `${esc(c.wx.place)} (${c.wx.unitT === 'f' ? '°F' : '°C'})` : 'not set', c.wx.place ? '' : 'no location') +
    row('Clock', (c.clock.h24 ? '24-hour' : '12-hour') + ' · ' + esc(c.clock.tzname)) +
    row('Refresh', `every ${c.refresh.min} min` + (c.refresh.quiet ? `, paused ${$('qt-start').selectedOptions[0].text}–${$('qt-end').selectedOptions[0].text}` : '')) +
    row('Blocks', state.installed.length ? esc(state.installed.join(', ')) : 'built-ins only');
}
async function finish() {
  const c = cfg();
  if (!c.wifi.ssid) { alertBox('fin-alert', 'danger', 'WiFi is required — go back to step 1.'); return; }
  busy('btn-finish', true, 'Saving…');
  try {
    const r = await api('/api/save', { method: 'POST', body: JSON.stringify(c) }, 3);
    if (!r.ok) { alertBox('fin-alert', 'danger', esc(r.msg || 'save failed')); busy('btn-finish', false); return; }
    await api('/api/finish', { method: 'POST' }, 1).catch(() => {});   // device reboots mid-response
    go(8);
  } catch (e) {
    // save may have gone through even if the reply got lost
    go(8);
  }
}

// ---------- init ----------
window.addEventListener('DOMContentLoaded', async () => {
  fillProviderSelect($('im-provider'), MAIL_PROVIDERS);
  fillProviderSelect($('cd-provider'), CAL_PROVIDERS);
  $('im-provider').addEventListener('change', () => { $('im-provider').dataset.touched = '1'; });
  fillTz(); fillHours(); providerChanged(); calProviderChanged(); showChanged();
  try {
    const st = await api('/api/state', null, 1);
    if (st && st.haveConfig) $('hdr-sub').textContent = 'Settings mode — existing configuration loaded';
    // device{} is the current shape; the flat panelW/panelH keep this page
    // working against firmware that predates it.
    applyDevice(Object.assign({ fw: st && st.fw, ap: st && st.ap }, (st && st.device) || {}));
    if (st && st.cfg) prefill(st.cfg);
  } catch (e) {}
});
// Size the layout-editor canvas to the panel's real aspect ratio
// (800x480 -> the classic 560x336; a 648x480 panel -> 560x415, etc.)
function setPanelAspect(w, h) {
  if (!w || !h) return;
  CY = Math.max(16, Math.round((560 * h / w) / ROWS));
  CX = Math.round(560 / COLS);
  const cv = $('ed-canvas');
  if (cv) {
    cv.style.width = (CX * COLS) + 'px';
    cv.style.height = (CY * ROWS) + 'px';
    // Cell guides follow the real grid instead of a baked-in 16x12.
    cv.style.backgroundImage =
      `repeating-linear-gradient(#0000 0 ${CY - 1}px,#00000014 ${CY - 1}px ${CY}px),` +
      `repeating-linear-gradient(90deg,#0000 0 ${CX - 1}px,#00000014 ${CX - 1}px ${CX}px)`;
  }
}
function prefill(c) {
  try {
    if (c.wifi) { $('wifi-ssid').value = c.wifi.ssid || ''; }
    if (c.imap && c.imap.user) {
      $('im-user').value = c.imap.user; $('im-host').value = c.imap.host || '';
      $('im-port').value = c.imap.port || 993; $('im-folder').value = c.imap.folder || 'INBOX';
      $('im-show').value = c.imap.show || 'unseen'; $('im-count').value = c.imap.count || 5;
      $('im-custom').value = c.imap.custom || '';
      // Stored settings are already resolved to a host, so show the entry that
      // matches it rather than re-deriving one from the address.
      const hit = MAIL_PROVIDERS.find(p => p.host && p.host === c.imap.host);
      $('im-provider').value = hit ? hit.id : 'custom';
      $('im-provider').dataset.touched = '1';
      providerChanged(); showChanged();
    }
    if (c.cal && c.cal.mode === 'ics') { $('cal-ics').checked = true; $('ics-url').value = c.cal.url || ''; calModeChanged(); }
    else if (c.cal && c.cal.mode === 'caldav') { $('cd-base').value = c.cal.url || ''; $('cd-user').value = c.cal.user || ''; state.calHref = c.cal.url || ''; }
    else if (c.cal && c.cal.mode === 'none') { $('cal-none').checked = true; calModeChanged(); }
    if (c.wx && c.wx.place) {
      state.wx = { name: c.wx.place, lat: c.wx.lat, lon: c.wx.lon, admin1: '', country: '', tz: '' };
      $('wx-picked').classList.remove('step-hidden');
      $('wx-picked').textContent = `Weather location: ${c.wx.place} (${c.wx.lat}, ${c.wx.lon})`;
      $('wx-unitT').value = c.wx.unitT || 'c'; $('wx-unitW').value = c.wx.unitW || 'mph';
    }
    if (c.clock) { $('ck-h24').value = c.clock.h24 ? '1' : '0'; if (c.clock.tzname) preselectTz(c.clock.tzname); }
    if (c.refresh) {
      $('rf-min').value = String(c.refresh.min || 5); $('qt-on').checked = !!c.refresh.quiet;
      $('qt-start').value = c.refresh.qs ?? 0; $('qt-end').value = c.refresh.qe ?? 6;
    }
  } catch (e) {}
}

// ================= tabs =================
document.querySelectorAll('#mainTabs a').forEach(a => a.onclick = (e) => {
  e.preventDefault();
  document.querySelectorAll('#mainTabs a').forEach(x => x.classList.remove('active'));
  a.classList.add('active');
  for (const t of ['setup', 'layout', 'blocks', 'update'])
    $(`tab-${t}`).classList.toggle('step-hidden', a.dataset.tab !== t);
  if (a.dataset.tab === 'layout') edInit();
  if (a.dataset.tab === 'blocks') blRefresh();
  if (a.dataset.tab === 'update') fwInfo();
});

// ================= layout editor =================
// Grid dimensions and cell pixels come from the device (see applyDevice).
let COLS = 16, ROWS = 12, CX = 35, CY = 28;
const BUILTINS = {
  'core-clock':      { name: 'Clock',          minW: 5, minH: 2 },
  'core-datestatus': { name: 'Date & status',  minW: 5, minH: 2 },
  'core-weather':    { name: 'Weather now',    minW: 5, minH: 4 },
  'core-forecast':   { name: '3-day forecast', minW: 5, minH: 3 },
  'core-calendar':   { name: 'Calendar',       minW: 6, minH: 3 },
  'core-inbox':      { name: 'Inbox',          minW: 6, minH: 3 },
};
let ed = { layout: [], blocks: [], sel: null, inited: false };

function blockMeta(id) {
  if (BUILTINS[id]) return BUILTINS[id];
  const b = ed.blocks.find(x => x.id === id);
  return b ? { name: b.name, minW: b.minW || 2, minH: b.minH || 2, params: b.params || [] }
           : { name: id, minW: 2, minH: 2 };
}

async function edInit() {
  if (ed.inited) return;
  ed.inited = true;
  try {
    const [lay, bl] = [await api('/api/layout'), await api('/api/blocks')];
    ed.layout = Array.isArray(lay) ? lay : [];
    ed.blocks = (bl && bl.blocks) || [];
  } catch (e) { alertBox('ed-alert', 'danger', 'Could not load layout from the device.'); }
  const sel = $('ed-add-sel');
  sel.innerHTML = '';
  for (const [id, m] of Object.entries(BUILTINS)) sel.add(new Option(m.name + ' (built-in)', id));
  for (const b of ed.blocks) sel.add(new Option(`${b.name} — ${b.author}`, b.id));
  edRender();
}

function edRender() {
  const cv = $('ed-canvas');
  cv.innerHTML = '';
  ed.layout.forEach((it, i) => {
    const m = blockMeta(it.block);
    const d = document.createElement('div');
    d.className = 'ed-tile';
    d.style.cssText = `position:absolute;left:${it.x * CX}px;top:${it.y * CY}px;` +
      `width:${it.w * CX - 2}px;height:${it.h * CY - 2}px;` +
      `background:${it.block.startsWith('core-') ? '#fff' : '#fdf0ee'};border:1.5px solid #1a1a1a;` +
      `border-radius:4px;font-size:11px;padding:3px 5px;cursor:grab;overflow:hidden;user-select:none;` +
      (ed.sel === i ? 'outline:3px solid #c0392b;' : '');
    d.innerHTML = `<b>${esc(m.name)}</b><div class="text-secondary">${it.w}×${it.h}</div>` +
      `<div style="position:absolute;right:0;bottom:0;width:14px;height:14px;cursor:nwse-resize;` +
      `background:linear-gradient(135deg,#0000 50%,#c0392b 50%)" data-rs="1"></div>`;
    d.onpointerdown = (e) => edDown(e, i, e.target.dataset.rs === '1');
    cv.appendChild(d);
  });
  edParamsPanel();
}

let drag = null;
function edDown(e, i, resize) {
  e.preventDefault();
  ed.sel = i;
  const it = ed.layout[i];
  drag = { i, resize, sx: e.clientX, sy: e.clientY, ox: it.x, oy: it.y, ow: it.w, oh: it.h, moved: false };
  window.onpointermove = edMove;
  window.onpointerup = edUp;
  edRender();
}
function edMove(e) {
  if (!drag) return;
  const it = ed.layout[drag.i], m = blockMeta(it.block);
  const dx = Math.round((e.clientX - drag.sx) / CX), dy = Math.round((e.clientY - drag.sy) / CY);
  if (dx || dy) drag.moved = true;
  if (drag.resize) {
    it.w = Math.max(m.minW || 2, Math.min(COLS - it.x, drag.ow + dx));
    it.h = Math.max(m.minH || 2, Math.min(ROWS - it.y, drag.oh + dy));
  } else {
    it.x = Math.max(0, Math.min(COLS - it.w, drag.ox + dx));
    it.y = Math.max(0, Math.min(ROWS - it.h, drag.oy + dy));
  }
  edRender();
}
function edUp() {
  if (drag) {
    const it = ed.layout[drag.i];
    const hit = ed.layout.some((o, j) => j !== drag.i &&
      it.x < o.x + o.w && o.x < it.x + it.w && it.y < o.y + o.h && o.y < it.y + it.h);
    if (hit) { it.x = drag.ox; it.y = drag.oy; it.w = drag.ow; it.h = drag.oh;
               alertBox('ed-alert', 'warning', 'Blocks can\'t overlap — move reverted.'); }
    else alertBox('ed-alert', '', '');
  }
  drag = null;
  window.onpointermove = window.onpointerup = null;
  edRender();
}

function edParamsPanel() {
  const p = $('ed-params');
  if (ed.sel === null || !ed.layout[ed.sel]) { p.classList.add('step-hidden'); return; }
  const it = ed.layout[ed.sel], m = blockMeta(it.block);
  let html = `<div class="d-flex justify-content-between"><b>${esc(m.name)}</b>` +
    `<button class="btn btn-outline-danger btn-sm" onclick="edRemove()">Remove from layout</button></div>`;
  (m.params || []).forEach(pr => {
    const val = (it.params && it.params[pr.key]) ?? pr.default ?? '';
    if (pr.type === 'choice') {
      const opts = (pr.choices || '').split(',').map(c =>
        `<option ${c === val ? 'selected' : ''}>${esc(c)}</option>`).join('');
      html += `<label class="form-label small mt-2">${esc(pr.label)}</label>` +
        `<select class="form-select form-select-sm" onchange="edParam('${pr.key}',this.value)">${opts}</select>`;
    } else {
      html += `<label class="form-label small mt-2">${esc(pr.label)}</label>` +
        `<input class="form-control form-control-sm" value="${esc(val)}" ` +
        `onchange="edParam('${pr.key}',this.value)">`;
    }
  });
  p.innerHTML = html;
  p.classList.remove('step-hidden');
}
function edParam(k, v) {
  const it = ed.layout[ed.sel];
  it.params = it.params || {};
  it.params[k] = v;
}
function edRemove() {
  ed.layout.splice(ed.sel, 1);
  ed.sel = null;
  edRender();
}
function edAdd() {
  const id = $('ed-add-sel').value, m = blockMeta(id);
  const w = m.minW || 3, h = m.minH || 2;
  for (let y = 0; y <= ROWS - h; y++) for (let x = 0; x <= COLS - w; x++) {
    const hit = ed.layout.some(o => x < o.x + o.w && o.x < x + w && y < o.y + o.h && o.y < y + h);
    if (!hit) {
      ed.layout.push({ inst: 'i' + Math.floor(performance.now() % 1e6), block: id, x, y, w, h, params: {} });
      ed.sel = ed.layout.length - 1;
      edRender();
      return;
    }
  }
  alertBox('ed-alert', 'warning', 'No free space — remove or shrink something first.');
}
async function edSave() {
  busy('btn-savelayout', true, 'Saving…');
  try {
    const r = await api('/api/layout', { method: 'POST', body: JSON.stringify(ed.layout) }, 2);
    alertBox('ed-alert', r.ok ? 'success' : 'danger', r.ok ? 'Layout saved — it draws on the next refresh (or press Preview).' : esc(r.msg));
  } catch (e) { alertBox('ed-alert', 'danger', 'Device didn\'t answer.'); }
  busy('btn-savelayout', false);
}
async function edPreview() {
  // Sends the CURRENT canvas: the device saves it, then renders it — so the
  // panel always shows exactly what you see here, even without pressing Save.
  busy('btn-preview', true, 'Saving & rendering…');
  try {
    const r = await api('/api/preview', { method: 'POST', body: JSON.stringify(ed.layout) }, 1);
    if (r.ok) alertBox('ed-alert', 'info', `Layout saved — the panel is drawing it now (about ${DEV.refreshSec} s).`);
    else alertBox('ed-alert', 'danger', esc(r.msg || 'Layout rejected.'));
  } catch (e) { alertBox('ed-alert', 'danger', 'Device didn\'t answer.'); }
  setTimeout(() => busy('btn-preview', false), 3000);
}

// ================= blocks manager =================
async function blRefresh() {
  try {
    const r = await api('/api/blocks');
    ed.blocks = (r && r.blocks) || [];
    $('bl-unsigned').checked = !!(r && r.allowUnsigned);
    const L = $('bl-list');
    L.innerHTML = '';
    if (!ed.blocks.length)
      L.innerHTML = '<div class="list-group-item text-secondary small">No contributed blocks installed yet — the built-ins are always available in the Layout tab.</div>';
    for (const b of ed.blocks) {
      const d = document.createElement('div');
      d.className = 'list-group-item d-flex justify-content-between align-items-center';
      const badge = b.sigOk
        ? `<span class="badge text-bg-success">signed · ${esc(b.keyid)}</span>`
        : '<span class="badge text-bg-warning">unsigned</span>';
      d.innerHTML = `<span><b>${esc(b.name)}</b> <span class="text-secondary small">v${esc(b.version)} — ${esc(b.author)}</span> ${badge}</span>` +
        `<button class="btn btn-outline-danger btn-sm" onclick="blRemove('${esc(b.id)}')">Remove</button>`;
      L.appendChild(d);
    }
    ed.inited = false;   // palette refresh next time layout opens
  } catch (e) { alertBox('bl-alert', 'danger', 'Could not reach the device.'); }
}
async function blRemove(id) {
  await api('/api/blocks/remove', { method: 'POST', body: JSON.stringify({ id }) }, 2).catch(() => {});
  blRefresh();
}
async function blPolicy() {
  await api('/api/blocks/policy', { method: 'POST', body: JSON.stringify({ allowUnsigned: $('bl-unsigned').checked }) }, 2).catch(() => {});
}
function blResult(r) {
  if (r.ok) {
    const sig = r.sigOk ? `signature verified (${esc(r.keyid)})` : 'installed UNSIGNED';
    alertBox('bl-alert', r.sigOk ? 'success' : 'warning', `Block installed — ${sig}. Add it in the Layout tab.`);
  } else alertBox('bl-alert', 'danger', esc(r.msg || 'install failed'));
  blRefresh();
}
async function blInstallUrl() {
  const url = $('bl-url').value.trim();
  if (!url) return;
  try { blResult(await api('/api/blocks/install', { method: 'POST', body: JSON.stringify({ url }) }, 2)); }
  catch (e) { alertBox('bl-alert', 'danger', 'Device didn\'t answer.'); }
}
async function blInstallPaste() {
  const content = $('bl-paste').value.trim();
  if (!content) return;
  try { blResult(await api('/api/blocks/install', { method: 'POST', body: JSON.stringify({ content }) }, 2)); }
  catch (e) { alertBox('bl-alert', 'danger', 'Device didn\'t answer.'); }
}
async function blRegistry() {
  const url = $('bl-reg-url').value.trim();
  if (!url) return;
  try {
    const r = await api('/api/registry?url=' + encodeURIComponent(url), null, 2);
    const L = $('bl-reg-list');
    L.innerHTML = '';
    if (!r.ok) { alertBox('bl-alert', 'danger', esc(r.msg)); return; }
    alertBox('bl-alert', r.sigOk ? 'success' : 'warning',
      r.sigOk ? `Registry index signature verified (${esc(r.keyid)}).` : 'Registry index is NOT signed by a trusted key.');
    for (const b of (r.blocks || [])) {
      const d = document.createElement('div');
      d.className = 'list-group-item d-flex justify-content-between align-items-center';
      d.innerHTML = `<span><b>${esc(b.name)}</b> <span class="text-secondary small">${esc(b.description || '')}</span></span>` +
        `<button class="btn btn-outline-dark btn-sm" onclick='blInstallFromReg(${JSON.stringify(b.epb)})'>Install</button>`;
      L.appendChild(d);
    }
  } catch (e) { alertBox('bl-alert', 'danger', 'Device didn\'t answer.'); }
}
async function blInstallFromReg(url) {
  try { blResult(await api('/api/blocks/install', { method: 'POST', body: JSON.stringify({ url }) }, 2)); }
  catch (e) { alertBox('bl-alert', 'danger', 'Device didn\'t answer.'); }
}

// ================= firmware update (OTA) =================
async function fwInfo() {
  try {
    const r = await api('/api/ota/info', null, 2);
    $('fw-status').innerHTML =
      `Running <b>v${esc(r.version)}</b> from slot <span class="mono">${esc(r.running)}</span>` +
      (r.otaReady ? ` — spare slot ready (${(r.slotBytes / 1048576).toFixed(1)} MB).`
                  : ' — <b>no spare slot</b>.');
    $('fw-form').classList.toggle('step-hidden', !r.otaReady);
    $('fw-migrate').classList.toggle('step-hidden', !!r.otaReady);
  } catch (e) { $('fw-status').textContent = 'Could not reach the device.'; }
}
function fwUpload() {
  const f = $('fw-file').files[0];
  if (!f) { alertBox('fw-alert', 'warning', 'Choose the .bin file first.'); return; }
  if (/factory/i.test(f.name))
    return alertBox('fw-alert', 'danger',
      'That looks like the <b>factory</b> image — over the air you must use <span class="mono">epaper-dashboard-app.bin</span>.');
  const sha = $('fw-sha').value.trim();
  if (sha && !/^[0-9a-fA-F]{64}$/.test(sha))
    return alertBox('fw-alert', 'danger', 'That SHA-256 doesn\'t look right (need 64 hex characters, or leave it blank).');
  busy('btn-fwup', true, 'Uploading…');
  $('fw-prog-wrap').classList.remove('step-hidden');
  const form = new FormData();
  form.append('firmware', f, f.name);
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/api/ota' + (sha ? '?sha256=' + sha : ''));
  xhr.upload.onprogress = (ev) => {
    if (!ev.lengthComputable) return;
    const pct = Math.round(100 * ev.loaded / ev.total);
    $('fw-prog').style.width = pct + '%';
    $('fw-prog').textContent = pct < 100 ? pct + '%' : 'flashing…';
  };
  xhr.onerror = () => {
    busy('btn-fwup', false);
    alertBox('fw-alert', 'danger', 'Upload failed — device didn\'t answer.');
  };
  xhr.onload = () => {
    busy('btn-fwup', false);
    let r = {};
    try { r = JSON.parse(xhr.responseText); } catch (e) {}
    if (r.ok) {
      $('fw-prog').style.width = '100%';
      $('fw-prog').textContent = 'done';
      alertBox('fw-alert', 'success',
        'Flashed and verified — the device is rebooting into the new firmware. ' +
        'It will redraw the dashboard and reopen this page in about a minute.');
    } else {
      $('fw-prog-wrap').classList.add('step-hidden');
      alertBox('fw-alert', 'danger', esc(r.msg || 'update failed'));
    }
  };
  xhr.send(form);
}
