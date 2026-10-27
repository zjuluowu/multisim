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

#ifndef LSG_UNSCRIPTED_H
#define LSG_UNSCRIPTED_H

#include <map>
#include <mutex>
#include <string>

namespace OHOS {
namespace SimGuard {
/*
 * Diagnostic record of calls into external boundaries that no scenario scripted.
 * Never an oracle: it only helps to see what the production code asked for.
 */
class Unscripted {
public:
    static void Record(const std::string &what);
    static std::map<std::string, int> Snapshot();
    static void Reset();
};
} // namespace SimGuard
} // namespace OHOS

#endif /* LSG_UNSCRIPTED_H */
