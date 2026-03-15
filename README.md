# Inbox Transfer

## MailCue Test Server

**Web UI:** http://localhost:8088

### Accounts

| Mailbox | Password |
|---------|----------|
| admin@mailcue.local | mailcue |
| inbox_a@mailcue.local | inboxpass123 |
| inbox_b@mailcue.local | inboxpass456 |

### Quick Start

```bash
podman compose up -d
./scripts/seed-mailcue.sh
```