#include "byoredis/server/commands.hh"
#include "byoredis/server/db.hh"
#include "byoredis/ds/hashtable.hh"
#include "byoredis/proto/tlv.hh"
#include "byoredis/ds/intrusive.hh"  // for container_of
#include "byoredis/ds/zset.hh"
#include <math.h>

void do_get(std::vector<std::string> &cmd, Buffer &buffer) {
  LookupKey key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash((uint8_t const *)key.key.data(), key.key.size());
  HNode *node = hm_lookup(&g_data.db, &key.node, &key_eq);
  if (!node) {
    return out_nil(buffer);
  }
  Entry *ent = container_of(node, Entry, node);
  if (ent->type != T_STR) {
    return out_err(buffer, ERR_BAD_TYP, "not a string value");
  }
  return out_str(buffer, ent->str.data(), ent->str.size());
}

void do_set(std::vector<std::string> &cmd, Buffer &buffer) {
  LookupKey key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash((uint8_t const *)key.key.data(), key.key.size());
  HNode *node = hm_lookup(&g_data.db, &key.node, &key_eq);
  if (node) {
    // found, update the value
    Entry *ent = container_of(node, Entry, node);
    if (ent->type != T_STR) {
      return out_err(buffer, ERR_BAD_TYP, "a non-string value exists");
    }
    ent->str.swap(cmd[2]);
  } else {
    // not found, allocate & insert a new pair
    Entry *ent = entry_new(T_STR);
    ent->key.swap(key.key);
    ent->node.hcode = key.node.hcode;
    ent->str.swap(cmd[2]);
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
    entry_free(container_of(node, Entry, node));
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

static bool str2dbl(std::string const &s, double &out) {
  char *endp = NULL;
  out = strtod(s.c_str(), &endp);
  return endp == s.c_str() + s.size() && !isnan(out);
}

static bool str2int(std::string const &s, int64_t &out) {
  char *endp = NULL;
  out = strtoll(s.c_str(), &endp, 10);
  return endp == s.c_str() + s.size();
}

// zadd zset score name
void do_zadd(std::vector<std::string> &cmd, Buffer &buffer) {
  double score = 0;
  if (!str2dbl(cmd[2], score)) {
    return out_err(buffer, ERR_BAD_ARG, "expect float");
  }
  // lookup or create the zset
  LookupKey key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash((uint8_t const *)key.key.data(), key.key.size());
  HNode *hnode = hm_lookup(&g_data.db, &key.node, &key_eq);

  Entry *ent = NULL;
  if (!hnode) {  // insert a new key
    ent = entry_new(T_ZSET);
    ent->key.swap(key.key);
    ent->node.hcode = key.node.hcode;
    hm_insert(&g_data.db, &ent->node);
  } else {      // check the existing key
    ent = container_of(hnode, Entry, node);
    if (ent->type != T_ZSET) {
      return out_err(buffer, ERR_BAD_TYP, "expect zset");
    }
  }

  // add or update the tuple
  std::string const &name = cmd[3];
  bool added = zset_insert(&ent->zset, name.data(), name.size(), score);
  return out_int(buffer, (int64_t)added);
}

static ZSet const EMPTY_ZSET;  // for key not exist; NULL for type mismatch

static ZSet * expect_zset(std::string &s) {
  LookupKey key;
  key.key.swap(s);
  key.node.hcode = str_hash((uint8_t const *)key.key.data(), key.key.size());
  HNode *hnode = hm_lookup(&g_data.db, &key.node, &key_eq);
  if (!hnode) {  // a non-existent key is treated as an empty zset
    return (ZSet *)&EMPTY_ZSET;
  }
  Entry *ent = container_of(hnode, Entry, node);
  return ent->type == T_ZSET ? &ent->zset : NULL;
}

// zrem zset name
void do_zrem(std::vector<std::string> &cmd, Buffer &buffer) {
  ZSet *zset = expect_zset(cmd[1]);
  if (!zset) {
    return out_err(buffer, ERR_BAD_TYP, "expect zset");
  }

  std::string const &name = cmd[2];
  ZNode *znode = zset_lookup(zset, name.data(), name.size());
  if (znode) {
    zset_delete(zset, znode);
  }
  return out_int(buffer, znode ? 1 : 0);
}

// zscore zset name
void do_zscore(std::vector<std::string> &cmd, Buffer &buffer) {
  ZSet *zset = expect_zset(cmd[1]);
  if (!zset) {
    return out_err(buffer, ERR_BAD_TYP, "expect zset");
  }

  std::string const &name = cmd[2];
  ZNode *znode = zset_lookup(zset, name.data(), name.size());
  return znode ? out_dbl(buffer, znode->score) : out_nil(buffer); 
}

// zquery zset score name offset limit
void do_zquery(std::vector<std::string> &cmd, Buffer &buffer) {
  // parse the arguments and lookup the KV pair
  double score = 0;
  if (!str2dbl(cmd[2], score)) {
    return out_err(buffer, ERR_BAD_ARG, "expect fp number");
  }
  std::string const &name = cmd[3];
  int64_t offset = 0, limit = 0;
  if (!str2int(cmd[4], offset) || !str2int(cmd[5], limit)) {
    return out_err(buffer, ERR_BAD_ARG, "expect int");
  }
  // get the zset
  ZSet *zset = expect_zset(cmd[1]);
  if (!zset) {
    return out_err(buffer, ERR_BAD_TYP, "expect zset");
  }
  // seek to the key
  if (limit <= 0) {
    return out_arr(buffer, 0); // empty array
  }
  ZNode *znode = zset_seekge(zset, score, name.data(), name.size());
  // offset
  znode = znode_offset(znode, offset);
  // iterate and output
  out_begin_arr(buffer);
  int64_t n = 0;
  while (znode && n < limit) {
    out_str(buffer, znode->name, znode->len);
    out_dbl(buffer, znode->score);
    znode = znode_offset(znode, +1);
    n++;
  }
  out_end_arr(buffer, (uint32_t)(n * 2));
}

// zrank zset name
void do_zrank(std::vector<std::string> &cmd, Buffer &buffer) {
  ZSet *zset = expect_zset(cmd[1]);
  if (!zset) {
    return out_err(buffer, ERR_BAD_TYP, "expect zset");
  }

  std::string const &name = cmd[2];
  ZNode *znode = zset_lookup(zset, name.data(), name.size());
  return znode ? out_int(buffer, avl_rank(&znode->tree)) : out_nil(buffer);
}

// zcount zset score1 name1 score2 name2(exclusive)
void do_zcount(std::vector<std::string> &cmd, Buffer &buffer) {
  double score1 = 0, score2 = 0;
  if (!str2dbl(cmd[2], score1) || !str2dbl(cmd[4], score2)) {
    return out_err(buffer, ERR_BAD_ARG, "expect fp number");
  }
  ZSet *zset = expect_zset(cmd[1]);
  if (!zset) {
    return out_err(buffer, ERR_BAD_TYP, "expect zset");
  }
  std::string const &name1 = cmd[3];
  std::string const &name2 = cmd[5];
  ZNode *znode1 = zset_seekge(zset, score1, name1.data(), name1.size());
  ZNode *znode2 = zset_seekge(zset, score2, name2.data(), name2.size());
  if (!znode1 || !znode2) {
    return out_int(buffer, 0);
  }
  int64_t rank1 = avl_rank(&znode1->tree);
  int64_t rank2 = avl_rank(&znode2->tree);
  int64_t count = rank2 - rank1;
  if (count < 0) {
    count = 0;
  }
  return out_int(buffer, count);
}
