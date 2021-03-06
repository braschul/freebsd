//===-- tsan_posix.cc -----------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file is a part of ThreadSanitizer (TSan), a race detector.
//
//===----------------------------------------------------------------------===//
#include "tsan_interface.h"
#include "tsan_test_util.h"
#include "gtest/gtest.h"
#include <pthread.h>

struct thread_key {
  pthread_key_t key;
  pthread_mutex_t *mtx;
  int val;
  int *cnt;
  thread_key(pthread_key_t key, pthread_mutex_t *mtx, int val, int *cnt)
    : key(key)
    , mtx(mtx)
    , val(val)
    , cnt(cnt) {
  }
};

static void thread_secific_dtor(void *v) {
  thread_key *k = (thread_key *)v;
  EXPECT_EQ(pthread_mutex_lock(k->mtx), 0);
  (*k->cnt)++;
  __tsan_write4(&k->cnt);
  EXPECT_EQ(pthread_mutex_unlock(k->mtx), 0);
  if (k->val == 42) {
    delete k;
  } else if (k->val == 43 || k->val == 44) {
    k->val--;
    EXPECT_EQ(pthread_setspecific(k->key, k), 0);
  } else {
    ASSERT_TRUE(false);
  }
}

static void *dtors_thread(void *p) {
  thread_key *k = (thread_key *)p;
  EXPECT_EQ(pthread_setspecific(k->key, k), 0);
  return 0;
}

TEST(Posix, ThreadSpecificDtors) {
  int cnt = 0;
  pthread_key_t key;
  EXPECT_EQ(pthread_key_create(&key, thread_secific_dtor), 0);
  pthread_mutex_t mtx;
  EXPECT_EQ(pthread_mutex_init(&mtx, 0), 0);
  pthread_t th[3];
  thread_key *k[3];
  k[0] = new thread_key(key, &mtx, 42, &cnt);
  k[1] = new thread_key(key, &mtx, 43, &cnt);
  k[2] = new thread_key(key, &mtx, 44, &cnt);
  EXPECT_EQ(pthread_create(&th[0], 0, dtors_thread, k[0]), 0);
  EXPECT_EQ(pthread_create(&th[1], 0, dtors_thread, k[1]), 0);
  EXPECT_EQ(pthread_join(th[0], 0), 0);
  EXPECT_EQ(pthread_create(&th[2], 0, dtors_thread, k[2]), 0);
  EXPECT_EQ(pthread_join(th[1], 0), 0);
  EXPECT_EQ(pthread_join(th[2], 0), 0);
  EXPECT_EQ(pthread_key_delete(key), 0);
  EXPECT_EQ(6, cnt);
}

static __thread int local_var;

static void *local_thread(void *p) {
  __tsan_write1(&local_var);
  __tsan_write1(&p);
  if (p == 0)
    return 0;
  const int kThreads = 4;
  pthread_t th[kThreads];
  for (int i = 0; i < kThreads; i++)
    EXPECT_EQ(pthread_create(&th[i], 0, local_thread,
              (void*)((long)p - 1)), 0);  // NOLINT
  for (int i = 0; i < kThreads; i++)
    EXPECT_EQ(pthread_join(th[i], 0), 0);
  return 0;
}

TEST(Posix, ThreadLocalAccesses) {
  local_thread((void*)2);
}

struct CondContext {
  pthread_mutex_t m;
  pthread_cond_t c;
  int data;
};

static void *cond_thread(void *p) {
  CondContext &ctx = *static_cast<CondContext*>(p);

  EXPECT_EQ(pthread_mutex_lock(&ctx.m), 0);
  EXPECT_EQ(ctx.data, 0);
  ctx.data = 1;
  EXPECT_EQ(pthread_cond_signal(&ctx.c), 0);
  EXPECT_EQ(pthread_mutex_unlock(&ctx.m), 0);

  EXPECT_EQ(pthread_mutex_lock(&ctx.m), 0);
  while (ctx.data != 2)
    EXPECT_EQ(pthread_cond_wait(&ctx.c, &ctx.m), 0);
  EXPECT_EQ(pthread_mutex_unlock(&ctx.m), 0);

  EXPECT_EQ(pthread_mutex_lock(&ctx.m), 0);
  ctx.data = 3;
  EXPECT_EQ(pthread_cond_broadcast(&ctx.c), 0);
  EXPECT_EQ(pthread_mutex_unlock(&ctx.m), 0);

  return 0;
}

TEST(Posix, CondBasic) {
  CondContext ctx;
  EXPECT_EQ(pthread_mutex_init(&ctx.m, 0), 0);
  EXPECT_EQ(pthread_cond_init(&ctx.c, 0), 0);
  ctx.data = 0;
  pthread_t th;
  EXPECT_EQ(pthread_create(&th, 0, cond_thread, &ctx), 0);

  EXPECT_EQ(pthread_mutex_lock(&ctx.m), 0);
  while (ctx.data != 1)
    EXPECT_EQ(pthread_cond_wait(&ctx.c, &ctx.m), 0);
  ctx.data = 2;
  EXPECT_EQ(pthread_mutex_unlock(&ctx.m), 0);
  EXPECT_EQ(pthread_cond_broadcast(&ctx.c), 0);

  EXPECT_EQ(pthread_mutex_lock(&ctx.m), 0);
  while (ctx.data != 3)
    EXPECT_EQ(pthread_cond_wait(&ctx.c, &ctx.m), 0);
  EXPECT_EQ(pthread_mutex_unlock(&ctx.m), 0);

  EXPECT_EQ(pthread_join(th, 0), 0);
  EXPECT_EQ(pthread_cond_destroy(&ctx.c), 0);
  EXPECT_EQ(pthread_mutex_destroy(&ctx.m), 0);
}
