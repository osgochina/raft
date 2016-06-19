#ifndef RAFT_LOG_H_
#define RAFT_LOG_H_
#include "raft.h"

//log_t 指针
typedef void* log_t;

//创建log对象
log_t* log_new();

//设置log对象的回调函数
void log_set_callbacks(log_t* me_, raft_cbs_t* funcs, void* raft);

//释放log对象
void log_free(log_t* me_);

//清楚log对象
void log_clear(log_t* me_);

/**
 * 添加到log
 * Add entry to log.
 * Don't add entry if we've already added this entry (based off ID)
 * Don't add entries with ID=0 
 * @return 0 if unsucessful; 1 otherwise */
int log_append_entry(log_t* me_, raft_entry_t* c);

/**
 * log对象中的log数量
 * @return number of entries held within log */
int log_count(log_t* me_);

/**
 * 删除指定索引的log
 * Delete all logs from this log onwards */
void log_delete(log_t* me_, int idx);

/**
 * 清空log
 * Empty the queue. */
void log_empty(log_t * me_);

/**
 * 清除上一个提交
 * Remove oldest entry
 * @return oldest entry */
void *log_poll(log_t * me_);

///根据索引值获取消息日志
raft_entry_t* log_get_from_idx(log_t* me_, int idx, int *n_etys);

raft_entry_t* log_get_at_idx(log_t* me_, int idx);

/**
 * 清除指定log更新的一次提交
 * @return youngest entry */
raft_entry_t *log_peektail(log_t * me_);

//删除指定log
void log_delete(log_t* me_, int idx);

//获取指定log对象的当前索引
int log_get_current_idx(log_t* me_);

#endif /* RAFT_LOG_H_ */
