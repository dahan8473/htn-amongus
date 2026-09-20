#include <Arduino.h>
#include <WiFi.h>
#include <esp_random.h>
#include <string.h>
#include "broadcast.h"
#include "espnow_radio.h"

#define PKT_MESH             0xA2
#define MESH_MAX_MSG         200  // payload cap, excludes the 6-byte header
#define MESH_INITIAL_TTL     6    // bounded well under max player count -- not an unbounded flood
#define SEEN_CACHE_SIZE      16
#define RELAY_QUEUE_SIZE     8
#define INBOX_SIZE           8
#define RELAY_JITTER_MIN_MS  5
#define RELAY_JITTER_MAX_MS  40

static uint8_t s_localTag[4]; // this badge's origin tag, derived from its MAC
static uint8_t s_seq = 0;

// ---- dedup: (origin, seq) pairs already delivered/relayed. This is what
// stops a flood from becoming a broadcast storm -- TTL alone only bounds how
// far a message can travel, not how many times each hop re-sends it. ----
struct SeenEntry {
  uint8_t originTag[4];
  uint8_t seq;
  bool used;
};
static SeenEntry seenCache[SEEN_CACHE_SIZE];
static int seenNext = 0;

static bool alreadySeen(const uint8_t *originTag, uint8_t seq) {
  for (int i = 0; i < SEEN_CACHE_SIZE; i++) {
    if (seenCache[i].used && seenCache[i].seq == seq && memcmp(seenCache[i].originTag, originTag, 4) == 0) {
      return true;
    }
  }
  return false;
}

static void markSeen(const uint8_t *originTag, uint8_t seq) {
  SeenEntry &e = seenCache[seenNext];
  memcpy(e.originTag, originTag, 4);
  e.seq = seq;
  e.used = true;
  seenNext = (seenNext + 1) % SEEN_CACHE_SIZE;
}

// ---- pending relays: rebroadcast after a small random delay so badges that
// all just heard the same packet don't collide by re-sending at once. ----
struct PendingRelay {
  bool active;
  unsigned long dueMs;
  uint8_t originTag[4];
  uint8_t seq;
  uint8_t ttl;
  char payload[MESH_MAX_MSG];
  int payloadLen;
};
static PendingRelay relayQueue[RELAY_QUEUE_SIZE];

static void scheduleRelay(const uint8_t *originTag, uint8_t seq, uint8_t ttl, const char *payload, int payloadLen) {
  for (int i = 0; i < RELAY_QUEUE_SIZE; i++) {
    if (relayQueue[i].active) {
      continue;
    }
    PendingRelay &r = relayQueue[i];
    r.active = true;
    r.dueMs = millis() + RELAY_JITTER_MIN_MS + (esp_random() % (RELAY_JITTER_MAX_MS - RELAY_JITTER_MIN_MS));
    memcpy(r.originTag, originTag, 4);
    r.seq = seq;
    r.ttl = ttl;
    r.payloadLen = payloadLen > MESH_MAX_MSG ? MESH_MAX_MSG : payloadLen;
    memcpy(r.payload, payload, r.payloadLen);
    return;
  }
  // relay queue full: message is still delivered locally, it just won't
  // propagate further from this badge specifically.
}

// ---- inbox: messages ready for the app to read via pollMessage() ----
struct InboxSlot {
  bool used;
  char msg[MESH_MAX_MSG + 1];
};
static InboxSlot inbox[INBOX_SIZE];

static void enqueueInbox(const char *payload, int len) {
  for (int i = 0; i < INBOX_SIZE; i++) {
    if (inbox[i].used) {
      continue;
    }
    int n = len > MESH_MAX_MSG ? MESH_MAX_MSG : len;
    memcpy(inbox[i].msg, payload, n);
    inbox[i].msg[n] = '\0';
    inbox[i].used = true;
    return;
  }
  // inbox full: drop -- pollMessage() drains it every loop() so this should
  // only happen under a burst far beyond normal game message rates.
}

static void sendWire(const uint8_t *originTag, uint8_t seq, uint8_t ttl, const char *payload, int payloadLen) {
  uint8_t buf[1 + 6 + MESH_MAX_MSG];
  buf[0] = PKT_MESH;
  memcpy(buf + 1, originTag, 4);
  buf[5] = seq;
  buf[6] = ttl;
  int len = payloadLen > MESH_MAX_MSG ? MESH_MAX_MSG : payloadLen;
  memcpy(buf + 7, payload, len);
  espNowSend(buf, 7 + len);
}

static void onMeshRecv(const uint8_t *mac, int rssi, const uint8_t *data, int len) {
  if (len < 6) {
    return;
  }
  const uint8_t *originTag = data;
  uint8_t seq = data[4];
  uint8_t ttl = data[5];
  if (alreadySeen(originTag, seq)) {
    return;
  }
  markSeen(originTag, seq);

  const char *payload = (const char *)(data + 6);
  int payloadLen = len - 6;
  enqueueInbox(payload, payloadLen);

  if (ttl > 0) {
    scheduleRelay(originTag, seq, ttl - 1, payload, payloadLen);
  }
}

void setupBroadcast() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  memcpy(s_localTag, mac + 2, 4); // plenty of entropy to tell badges apart
  espNowOnReceive(PKT_MESH, onMeshRecv);
}

void broadcastMessage(const char *msg) {
  uint8_t seq = s_seq++;
  markSeen(s_localTag, seq); // ignore our own message if a relay loops it back
  sendWire(s_localTag, seq, MESH_INITIAL_TTL, msg, strlen(msg));
}

int pollMessage(char *buf, int maxLen) {
  for (int i = 0; i < INBOX_SIZE; i++) {
    if (!inbox[i].used) {
      continue;
    }
    int n = (int)strlen(inbox[i].msg);
    if (n > maxLen - 1) {
      n = maxLen - 1;
    }
    memcpy(buf, inbox[i].msg, n);
    buf[n] = '\0';
    inbox[i].used = false;
    return n;
  }
  return 0;
}

void updateBroadcast() {
  unsigned long now = millis();
  for (int i = 0; i < RELAY_QUEUE_SIZE; i++) {
    if (relayQueue[i].active && now >= relayQueue[i].dueMs) {
      PendingRelay &r = relayQueue[i];
      sendWire(r.originTag, r.seq, r.ttl, r.payload, r.payloadLen);
      r.active = false;
    }
  }
}
