/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Declarations of OHOS musl extensions used by platform sources
 * (eventhandler, ipc). Implemented in env/src/libc_compat.c with glibc calls.
 */
#ifndef LSG_OHOS_LIBC_COMPAT_H
#define LSG_OHOS_LIBC_COMPAT_H

#include <stdint.h>
#include <sys/types.h>

#ifndef FDSAN_OWNER_TYPE_FILE
#define FDSAN_OWNER_TYPE_FILE 1
#endif

#ifdef __cplusplus
extern "C" {
#endif
int fdsan_close_with_tag(int fd, uint64_t tag);
void fdsan_exchange_owner_tag(int fd, uint64_t expected_tag, uint64_t new_tag);
uint64_t fdsan_create_owner_tag(int type, uint64_t tag);
pid_t getproctid(void);
#ifdef __cplusplus
}
#endif

#endif /* LSG_OHOS_LIBC_COMPAT_H */
