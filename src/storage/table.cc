//
// Created by kongsys on 9/5/19.
//

#include "table.h"
#include "logging.h"
#include <algorithm>

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::DEBUG;

namespace rtidb {
namespace storage {
bool Table::CheckTsValid(uint32_t index, int32_t ts_idx) {
    auto column_map_iter = column_key_map_.find(index);
    if (column_map_iter == column_key_map_.end()) {
        return false;
    }
    if (std::find(column_map_iter->second.cbegin(), column_map_iter->second.cend(), ts_idx)
                == column_map_iter->second.cend()) {
        PDLOG(WARNING, "ts cloumn not member of index, ts id %d index id %d, failed getting table tid %u pid %u", 
                    ts_idx, index, id_, pid_);
        return false;
    }
    return true;
}

int Table::InitColumnDesc() {
    if (table_meta_.column_desc_size() > 0) {
        uint32_t key_idx = 0;
        uint32_t ts_idx = 0;
        for (const auto &column_desc : table_meta_.column_desc()) {
            if (column_desc.add_ts_idx()) {
                mapping_.insert(std::make_pair(column_desc.name(), key_idx));
                key_idx++;
            } else if (column_desc.is_ts_col()) {
                ts_mapping_.insert(std::make_pair(column_desc.name(), ts_idx));
                if (column_desc.has_ttl()) {
                    ttl_vec_.push_back(std::make_shared<std::atomic<uint64_t>>(column_desc.ttl() * 60 * 1000));
                    new_ttl_vec_.push_back(std::make_shared<std::atomic<uint64_t>>(column_desc.ttl() * 60 * 1000));
                } else {
                    ttl_vec_.push_back(std::make_shared<std::atomic<uint64_t>>(table_meta_.ttl() * 60 * 1000));
                    new_ttl_vec_.push_back(std::make_shared<std::atomic<uint64_t>>(table_meta_.ttl() * 60 * 1000));
                }
                ts_idx++;
            }
        }
        if (ts_mapping_.size() > 1 && !mapping_.empty()) {
            mapping_.clear();
        }
        if (table_meta_.column_key_size() > 0) {
            mapping_.clear();
            key_idx = 0;
            for (const auto &column_key : table_meta_.column_key()) {
                uint32_t cur_key_idx = key_idx;
                std::string name = column_key.index_name();
                auto it = mapping_.find(name);
                if (it != mapping_.end()) {
                    return -1;
                }
                mapping_.insert(std::make_pair(name, key_idx));
                key_idx++;
                if (ts_mapping_.empty()) {
                    continue;
                }
                if (column_key_map_.find(cur_key_idx) == column_key_map_.end()) {
                    column_key_map_.insert(std::make_pair(cur_key_idx, std::vector<uint32_t>()));
                }
                if (ts_mapping_.size() == 1) {
                    column_key_map_[cur_key_idx].push_back(ts_mapping_.begin()->second);
                    continue;
                }
                for (const auto &ts_name : column_key.ts_name()) {
                    auto ts_iter = ts_mapping_.find(ts_name);
                    if (ts_iter == ts_mapping_.end()) {
                        PDLOG(WARNING, "not found ts_name[%s]. tid %u pid %u",
                              ts_name.c_str(), id_, pid_);
                        return -1;
                    }
                    if (std::find(column_key_map_[cur_key_idx].begin(), column_key_map_[cur_key_idx].end(),
                                  ts_iter->second) == column_key_map_[cur_key_idx].end()) {
                        column_key_map_[cur_key_idx].push_back(ts_iter->second);
                    }
                }
            }
        } else {
            if (!ts_mapping_.empty()) {
                for (const auto &kv : mapping_) {
                    uint32_t cur_key_idx = kv.second;
                    if (column_key_map_.find(cur_key_idx) == column_key_map_.end()) {
                        column_key_map_.insert(std::make_pair(cur_key_idx, std::vector<uint32_t>()));
                    }
                    uint32_t cur_ts_idx = ts_mapping_.begin()->second;
                    if (std::find(column_key_map_[cur_key_idx].begin(), column_key_map_[cur_key_idx].end(),
                                  cur_ts_idx) == column_key_map_[cur_key_idx].end()) {
                        column_key_map_[cur_key_idx].push_back(cur_ts_idx);
                    }
                }
            }
        }

        if (ts_idx > UINT8_MAX) {
            PDLOG(INFO, "failed create table because ts column more than %d, tid %u pid %u", UINT8_MAX + 1, id_, pid_);
        }
    } else {
        for (int32_t i = 0; i < table_meta_.dimensions_size(); i++) {
            mapping_.insert(std::make_pair(table_meta_.dimensions(i), (uint32_t) i));
            PDLOG(INFO, "add index name %s, idx %d to table %s, tid %u, pid %u",
                  table_meta_.dimensions(i).c_str(), i, table_meta_.name().c_str(), id_, pid_);
        }
    }
    // add default dimension
    if (mapping_.empty()) {
        mapping_.insert(std::make_pair("idx0", 0));
        PDLOG(INFO, "no index specified with default");
    }
    return 0;
}
}
}