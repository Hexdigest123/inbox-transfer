#!/bin/bash

set -e

BASE_URL="${MAILCUE_URL:-http://localhost:8088}"
ADMIN_USER="${MAILCUE_ADMIN_USER:-admin}"
ADMIN_PASSWORD="${MAILCUE_ADMIN_PASSWORD:-mailcue}"
DOMAIN="${MAILCUE_DOMAIN:-mailcue.local}"

log_info() { printf '\033[0;32m[INFO]\033[0m %s\n' "$1"; }
log_error() { printf '\033[0;31m[ERROR]\033[0m %s\n' "$1"; }

wait_for_healthy() {
    log_info "Waiting for MailCue..."
    local i
    for i in $(seq 1 30); do
        if curl -s "${BASE_URL}/api/v1/health" > /dev/null 2>&1; then
            log_info "MailCue is healthy!"
            return 0
        fi
        sleep 2
    done
    log_error "MailCue health check failed"
    return 1
}

login() {
    log_info "Logging in as ${ADMIN_USER}..."
    local response
    response=$(curl -s -X POST "${BASE_URL}/api/v1/auth/login" \
        -H "Content-Type: application/json" \
        -d "{\"username\":\"${ADMIN_USER}\",\"password\":\"${ADMIN_PASSWORD}\"}")
    
    TOKEN=$(printf '%s' "$response" | jq -r '.access_token // empty')
    if [ -z "$TOKEN" ]; then
        log_error "No access token in response: $response"
        return 1
    fi
    log_info "Authenticated successfully"
}

create_mailbox() {
    local address="$1"
    local password="$2"
    
    log_info "Creating mailbox: ${address}"
    local result
    result=$(curl -s -w "%{http_code}" -X POST "${BASE_URL}/api/v1/mailboxes" \
        -H "Authorization: Bearer ${TOKEN}" \
        -H "Content-Type: application/json" \
        -d "{\"address\":\"${address}\",\"password\":\"${password}\"}" 2>&1) || true
    
    local http_code="${result##*$'\n'}"   
    if [ "$http_code" = "200" ] || [ "$http_code" = "201" ]; then
        log_info "Mailbox ${address} created"
    else
        log_info "Mailbox ${address} already exists or created"
    fi
}

bulk_inject() {
    local payload="$1"
    local response
    response=$(curl -s -X POST "${BASE_URL}/api/v1/emails/bulk-inject" \
        -H "Authorization: Bearer ${TOKEN}" \
        -H "Content-Type: application/json" \
        -d "$payload")
    
    local injected
    injected=$(printf '%s' "$response" | jq -r '.injected // 0')
    if [ "$injected" -gt 0 ]; then
        log_info "Injected ${injected} emails successfully"
        return 0
    else
        log_error "Bulk inject failed: $response"
        return 1
    fi
}

main() {
    log_info "Starting MailCue seeding..."
    log_info "API URL: ${BASE_URL}"
    log_info "Domain: ${DOMAIN}"
    
    wait_for_healthy
    login
    
    local inbox_a="inbox_a@${DOMAIN}"
    local inbox_b="inbox_b@${DOMAIN}"
    
    create_mailbox "$inbox_a" "inboxpass123"
    create_mailbox "$inbox_b" "inboxpass456"
    
    log_info "Injecting test emails..."
    
    local payload
    payload=$(cat <<EOF
{
  "emails": [
    {
      "mailbox": "${inbox_a}",
      "from_address": "sender@example.com",
      "to_addresses": ["${inbox_a}"],
      "subject": "Simple plain text email",
      "text_body": "This is a simple plain text email.\n\nNothing fancy here, just testing basic functionality.\n\nBest regards,\nSender"
    },
    {
      "mailbox": "${inbox_a}",
      "from_address": "marketing@company.com",
      "to_addresses": ["${inbox_a}"],
      "subject": "HTML newsletter with styling",
      "text_body": "This is the text version of our newsletter.",
      "html_body": "<html><body><h1>Newsletter</h1><p>This is an <strong>HTML newsletter</strong> with styled content.</p></body></html>"
    },
    {
      "mailbox": "${inbox_a}",
      "from_address": "international@example.de",
      "to_addresses": ["${inbox_a}"],
      "subject": "Unicode test: émojis 🎉 and Ñoñó",
      "text_body": "Unicode handling test:\n- German: Ä Ö Ü ß\n- French: é è ê ë\n- Emojis: 🎉 🚀 ✅ ❤️ 📧\n- Japanese: 日本語\n- Chinese: 中文测试"
    },
    {
      "mailbox": "${inbox_a}",
      "from_address": "noreply@system.internal",
      "to_addresses": ["${inbox_a}"],
      "subject": "",
      "text_body": "This email has no subject line. Testing empty subject handling."
    },
    {
      "mailbox": "${inbox_a}",
      "from_address": "longemail@subdomain.example.com",
      "to_addresses": ["${inbox_a}"],
      "subject": "This is a very long subject line that tests how the system handles subjects that exceed typical display limits",
      "text_body": "Testing long subject handling."
    },
    {
      "mailbox": "${inbox_a}",
      "from_address": "notifications@service.com",
      "to_addresses": ["${inbox_a}"],
      "subject": "Multi-recipient email",
      "text_body": "This email has multiple recipients in the To field."
    },
    {
      "mailbox": "${inbox_a}",
      "from_address": "team@company.com",
      "to_addresses": ["${inbox_a}"],
      "subject": "Email with CC recipients",
      "text_body": "This email has CC recipients.",
      "cc_addresses": ["manager@company.com", "hr@company.com"]
    },
    {
      "mailbox": "${inbox_a}",
      "from_address": "alice@company.com",
      "to_addresses": ["${inbox_a}"],
      "subject": "Re: Project discussion",
      "text_body": "This is a reply in a thread.\n\n> Previous message here\n\nBest,\nAlice"
    },
    {
      "mailbox": "${inbox_a}",
      "from_address": "security@bank.com",
      "to_addresses": ["${inbox_a}"],
      "subject": "URGENT: Account Alert!!!",
      "text_body": "Dear Customer,\n\nThis tests spam-like characteristics.\n\nSPAM INDICATORS: URGENT, EXCLAMATIONS"
    },
    {
      "mailbox": "${inbox_a}",
      "from_address": "minimal@test.com",
      "to_addresses": ["${inbox_a}"],
      "subject": ".",
      "text_body": "."
    },
    {
      "mailbox": "${inbox_a}",
      "from_address": "spammer@test.com",
      "to_addresses": ["${inbox_a}"],
      "subject": "LOTS OF WHITESPACE",
      "text_body": "\n\n\n\nLots of whitespace.\n\n\n\n"
    },
    {
      "mailbox": "${inbox_a}",
      "from_address": "forwarder@company.com",
      "to_addresses": ["${inbox_a}"],
      "subject": "Fwd: Original Subject",
      "text_body": "---------- Forwarded message ----------\nFrom: original@sender.com\nSubject: Original Subject\n\nThis is forwarded content."
    }
  ]
}
EOF
)
    
    bulk_inject "$payload"
    
    log_info "Seeding complete!"
    printf '\n'
    log_info "Created mailboxes:"
    printf '  - %s (password: inboxpass123)\n' "$inbox_a"
    printf '  - %s (password: inboxpass456)\n' "$inbox_b"
    printf '\n'
    log_info "Web UI: ${BASE_URL}"
    log_info "Admin: ${ADMIN_USER} / ${ADMIN_PASSWORD}"
}

main "$@"