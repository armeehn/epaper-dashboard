#pragma once
// Minimal IMAP4rev1 client over TLS: login, select, search, fetch headers.
// Only what the dashboard needs — small, with clear staged errors.
#include <Arduino.h>
#include "dashboard_data.h"
#include "settings.h"

// Build the IMAP SEARCH criteria from the settings "show" mode.
String imapCriteria(const Settings& s);

// Parse one raw RFC822 header block (From/Subject/Date) into an EmailT.
// Pure function — unit-testable on host.
void imapParseHeaderBlock(const char* block, EmailT& out);

// Connect + login + count unseen + fetch newest N matching messages.
bool imapFetch(const Settings& s, ImapResult& r);
