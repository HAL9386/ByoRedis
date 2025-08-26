#include "byoredis/server/commands.hh"
#include "byoredis/server/db.hh"
#include "byoredis/ds/hashtable.hh"
#include "byoredis/proto/tlv.hh"
#include "byoredis/ds/intrusive.hh"  // for container_of

void do_get(std::vector<std::string> &cmd, Buffer &buffer) {
  LookupKey key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash((uint8_t const *)key.key.data(), key.key.size());
  HNode *node = hm_lookup(&g_data.db, &key.node, &key_eq);
  if (!node) {
    return out_nil(buffer);
  }
  std::string const &val = container_of(node, struct Entry, node)->val;
  return out_str(buffer, val.data(), val.size());
}

void do_set(std::vector<std::string> &cmd, Buffer &buffer) {
  LookupKey key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash((uint8_t const *)key.key.data(), key.key.size());
  HNode *node = hm_lookup(&g_data.db, &key.node, &key_eq);
  if (node) {
    // found, update the value
    container_of(node, struct Entry, node)->val.swap(cmd[2]);
  } else {
    // not found, allocate & insert a new pair
    Entry *ent = new Entry();
    ent->key.swap(key.key);
    ent->val.swap(cmd[2]);
    ent->node.hcode = key.node.hcode;
    hm_insert(&g_data.db, &ent->node);
  }
  return out_nil(buffer);
}

void do_del(std::vector<std::string> &cmd, Buffer &buffer) {
  LookupKey key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash((uint8_t const *)key.key.data(), key.key.size());
  HNode *node = hm_delete(&g_data.db, &key.node, &key_eq);
  if (node) {  // deallocate the pair if found
    delete container_of(node, struct Entry, node);
  }
  return out_int(buffer, node ? 1 : 0);  // the number of deleted keys
}

static bool cb_keys(HNode *node, void *arg) {
  Buffer &buf = *(Buffer *)arg;
  std::string const &key = container_of(node, struct Entry, node)->key;
  out_str(buf, key.data(), key.size()); 
  return true;
}

void do_keys(std::vector<std::string> &, Buffer &buffer) {
  out_arr(buffer, (uint32_t)hm_size(&g_data.db));
  hm_foreach(&g_data.db, &cb_keys, (void *)&buffer);
}
