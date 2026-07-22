// E-Paper Dashboard setup wizard
'use strict';

const $ = (id) => document.getElementById(id);
const steps = 8;
let cur = 0;

const state = {
  wifiOk: false,
  calHref: '',       // chosen CalDAV calendar URL
  calName: '',
  wx: null,          // {name, lat, lon, tz}
};

// ---------- providers ----------
const PROVIDERS = {
  migadu:   { host: 'imap.migadu.com',  dav: 'https://cdav.migadu.com/',
              hint: 'Your normal Migadu mailbox password (or an app password if you created one).' },
  fastmail: { host: 'imap.fastmail.com', dav: 'https://caldav.fastmail.com/',
              hint: 'Use an app password: Fastmail Settings → Privacy & Security → App passwords.' },
  gmail:    { host: 'imap.gmail.com',    dav: '',
              hint: 'Requires an app password (Google Account → Security → 2-Step Verification → App passwords). For calendar, use the ICS secret address instead of CalDAV.' },
  custom:   { host: '', dav: '', hint: '' },
};

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
  window.scrollTo(0, 0);
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
function providerChanged() {
  const p = PROVIDERS[$('im-provider').value];
  if (p.host) $('im-host').value = p.host;
  $('im-pass-hint').textContent = p.hint;
  if (p.dav) $('cd-base').value = p.dav;
  emailChanged();
}
function emailChanged() {
  const u = $('im-user').value.trim();
  if ($('im-provider').value === 'custom' && u.includes('@') && !$('im-host').value)
    $('im-host').value = 'imap.' + u.split('@')[1];
  if (u) $('cd-user').value = u;
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

// ---------- step 6: review + save ----------
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
    row('WiFi', esc(c.wifi.ssid) || '—', !state.wifiOk && c.wifi.ssid ? 'not tested' : '') +
    row('Email', c.imap.user ? `${esc(c.imap.user)} <span class="mono">(${esc(c.imap.host)})</span>` : 'disabled') +
    row('Calendar', c.cal.mode === 'none' ? 'disabled' : `${c.cal.mode.toUpperCase()}: <span class="mono">${esc((state.calName || c.cal.url).slice(0, 60))}</span>`) +
    row('Weather', c.wx.place ? `${esc(c.wx.place)} (${c.wx.unitT === 'f' ? '°F' : '°C'})` : 'not set', c.wx.place ? '' : 'no location') +
    row('Clock', (c.clock.h24 ? '24-hour' : '12-hour') + ' · ' + esc(c.clock.tzname)) +
    row('Refresh', `every ${c.refresh.min} min` + (c.refresh.quiet ? `, paused ${$('qt-start').selectedOptions[0].text}–${$('qt-end').selectedOptions[0].text}` : ''));
}
async function finish() {
  const c = cfg();
  if (!c.wifi.ssid) { alertBox('fin-alert', 'danger', 'WiFi is required — go back to step 1.'); return; }
  busy('btn-finish', true, 'Saving…');
  try {
    const r = await api('/api/save', { method: 'POST', body: JSON.stringify(c) }, 3);
    if (!r.ok) { alertBox('fin-alert', 'danger', esc(r.msg || 'save failed')); busy('btn-finish', false); return; }
    await api('/api/finish', { method: 'POST' }, 1).catch(() => {});   // device reboots mid-response
    go(7);
  } catch (e) {
    // save may have gone through even if the reply got lost
    go(7);
  }
}

// ---------- init ----------
window.addEventListener('DOMContentLoaded', async () => {
  fillTz(); fillHours(); providerChanged(); showChanged();
  try {
    const st = await api('/api/state', null, 1);
    if (st && st.haveConfig) $('hdr-sub').textContent = 'Settings mode — existing configuration loaded';
    if (st && st.cfg) prefill(st.cfg);
  } catch (e) {}
});
function prefill(c) {
  try {
    if (c.wifi) { $('wifi-ssid').value = c.wifi.ssid || ''; }
    if (c.imap && c.imap.user) {
      $('im-user').value = c.imap.user; $('im-host').value = c.imap.host || '';
      $('im-port').value = c.imap.port || 993; $('im-folder').value = c.imap.folder || 'INBOX';
      $('im-show').value = c.imap.show || 'unseen'; $('im-count').value = c.imap.count || 5;
      $('im-custom').value = c.imap.custom || ''; $('im-provider').value = 'custom'; showChanged();
    }
    if (c.cal && c.cal.mode === 'ics') { $('cal-ics').checked = true; $('ics-url').value = c.cal.url || ''; calModeChanged(); }
    else if (c.cal && c.cal.mode === 'caldav') { $('cd-base').value = c.cal.url || ''; $('cd-user').value = c.cal.user || ''; state.calHref = c.cal.url || ''; }
    else if (c.cal && c.cal.mode === 'none') { $('cal-none').checked = true; calModeChanged(); }
    if (c.wx && c.wx.place) {
      state.wx = { name: c.wx.place, lat: c.wx.lat, lon: c.wx.lon, admin1: '', country: '', tz: '' };
      $('wx-picked').classList.remove('step-hidden');
      $('wx-picked').textContent = `Weather location: ${c.wx.place} (${c.wx.lat}, ${c.wx.lon})`;
      $('wx-unitT').value = c.wx.unitT || 'f'; $('wx-unitW').value = c.wx.unitW || 'mph';
    }
    if (c.clock) { $('ck-h24').value = c.clock.h24 ? '1' : '0'; if (c.clock.tzname) preselectTz(c.clock.tzname); }
    if (c.refresh) {
      $('rf-min').value = String(c.refresh.min || 5); $('qt-on').checked = !!c.refresh.quiet;
      $('qt-start').value = c.refresh.qs ?? 0; $('qt-end').value = c.refresh.qe ?? 6;
    }
  } catch (e) {}
}
