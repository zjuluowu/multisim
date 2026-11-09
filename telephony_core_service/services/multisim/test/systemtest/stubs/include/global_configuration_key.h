/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#ifndef OHOS_ABILITY_BASE_GLOBAL_CONFIGURATION_KEY_H
#define OHOS_ABILITY_BASE_GLOBAL_CONFIGURATION_KEY_H

#include <string>

/*
 * Single source of truth for all configuration keys.
 *
 * To add a new key, append one line to CONFIGURATION_KEY_LIST below.
 * The list is expanded by consumers via X-Macro to generate both the
 * constant declarations and the whitelist used by MakeTheKey, so adding
 * a key no longer requires editing multiple files.
 *
 * X(name, value) where:
 *   name  - the C++ identifier used as the constant name
 *   value - the string literal key value
 */
#define CONFIGURATION_KEY_LIST(X) \
    X(SYSTEM_LANGUAGE,              "ohos.system.language") \
    X(IS_PREFERRED_LANGUAGE,        "ohos.system.isPreferredLanguage") \
    X(SYSTEM_LOCALE,                "ohos.system.locale") \
    X(SYSTEM_HOUR,                  "ohos.system.hour") \
    X(SYSTEM_COLORMODE,             "ohos.system.colorMode") \
    X(INPUT_POINTER_DEVICE,         "input.pointer.device") \
    X(DEVICE_TYPE,                  "const.build.characteristics") \
    X(COLORMODE_IS_SET_BY_APP,      "ohos.system.colorMode.isSetByApp") \
    X(COLORMODE_IS_SET_BY_SA,       "ohos.system.colorMode.isSetBySa") \
    X(THEME,                        "ohos.application.theme") \
    X(THEME_ID,                     "ohos.application.themeId") \
    X(THEME_ICON,                   "ohos.application.themeIcon") \
    X(THEME_SKIN,                   "ohos.application.themeSkin") \
    X(SYSTEM_FONT_ID,               "ohos.system.fontId") \
    X(SYSTEM_FONT_SIZE_SCALE,       "ohos.system.fontSizeScale") \
    X(SYSTEM_FONT_WEIGHT_SCALE,     "ohos.system.fontWeightScale") \
    X(SYSTEM_MCC,                   "ohos.system.mcc") \
    X(SYSTEM_MNC,                   "ohos.system.mnc") \
    X(APPLICATION_FONT,             "ohos.application.font") \
    X(APP_FONT_SIZE_SCALE,          "ohos.app.fontSizeScale") \
    X(APP_FONT_MAX_SCALE,           "ohos.app.fontMaxScale") \
    X(SYSTEM_SMART_GESTURE_SWITCH,  "ohos.system.smartGesture") \
    /* ConfigurationInner app/display-level keys (merged for single source) */ \
    X(APPLICATION_DIRECTION,        "ohos.application.direction") \
    X(APPLICATION_DENSITYDPI,       "ohos.application.densitydpi") \
    X(APPLICATION_DISPLAYID,        "ohos.application.displayid")

namespace OHOS {
namespace AAFwk {
namespace GlobalConfigurationKey {
    /* For the time being, there is no uniform standard */

#define DECLARE_KEY(name, value) constexpr const char* name = value;
    CONFIGURATION_KEY_LIST(DECLARE_KEY)
#undef DECLARE_KEY

} // namespace GlobalConfigurationKey
} // namespace AAFwk
} // namespace OHOS
#endif // OHOS_ABILITY_BASE_GLOBAL_CONFIGURATION_KEY_H
